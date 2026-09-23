#include "kernel_memory/initializer.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/sockets.h"
#include "utils/swap_km.h"

static void init_block_list(t_swap_data* swap_data);
static int count_instructions(FILE* f);

t_kernel_memory_data* init_kernel_memory_data(
    t_socket* socket_kernel_memory, char* scripts_basepath,
    int instruction_delay, int compaction_delay, int segment_max_size,
    t_allocation_strategy allocation_strategy, t_log* logger)
{
  t_kernel_memory_data* kernel_data = malloc(sizeof(t_kernel_memory_data));
  kernel_data->socket_kernel_memory = socket_kernel_memory;
  kernel_data->logger = logger;
  atomic_init(&(kernel_data->socket_scheduler), NULL);
  kernel_data->scripts_basepath = scripts_basepath;
  kernel_data->instruction_delay = instruction_delay;
  kernel_data->compaction_delay = compaction_delay;
  kernel_data->segment_max_size = segment_max_size;
  kernel_data->allocation_strategy = allocation_strategy;
  kernel_data->connected_sticks = list_create();
  kernel_data->connected_cpus = list_create();
  kernel_data->processes = list_create();
  kernel_data->main_memory =
      init_main_memory(segment_max_size, allocation_strategy, compaction_delay);
  atomic_init(&(kernel_data->swap_data), NULL);
  kernel_data->processes_mutex = malloc(sizeof(mtx_t));
  kernel_data->socket_list_mutex = malloc(sizeof(mtx_t));
  mtx_init(kernel_data->processes_mutex, mtx_plain);
  mtx_init(kernel_data->socket_list_mutex, mtx_plain);
  kernel_data->active_threads = 0;
  kernel_data->active_threads_mutex = malloc(sizeof(mtx_t));
  kernel_data->active_threads_cond = malloc(sizeof(cnd_t));
  mtx_init(kernel_data->active_threads_mutex, mtx_plain);
  cnd_init(kernel_data->active_threads_cond);
  return kernel_data;
}

t_scheduler_data* init_scheduler_data(
    t_socket* socket_kernel_memory, t_socket* socket_scheduler,
    t_list* processes, char* scripts_basepath, mtx_t* processes_mutex,
    t_main_memory* main_memory, t_list* connected_sticks, mtx_t* sticks_mutex,
    _Atomic(t_swap_data*)* swap_data, t_log* logger, int* active_threads,
    mtx_t* active_threads_mutex, cnd_t* active_threads_cond)
{
  t_scheduler_data* scheduler_data = malloc(sizeof(t_scheduler_data));
  scheduler_data->socket_kernel_memory = socket_kernel_memory;
  scheduler_data->socket_scheduler = socket_scheduler;
  scheduler_data->processes_mutex = processes_mutex;
  scheduler_data->logger = logger;
  scheduler_data->processes = processes;
  scheduler_data->scripts_basepath = scripts_basepath;
  scheduler_data->connected_sticks = connected_sticks;
  scheduler_data->socket_list_mutex = sticks_mutex;
  scheduler_data->main_memory = main_memory;
  scheduler_data->swap_data = swap_data;
  scheduler_data->active_threads = active_threads;
  scheduler_data->active_threads_mutex = active_threads_mutex;
  scheduler_data->active_threads_cond = active_threads_cond;
  return scheduler_data;
}

t_cpu_data* init_cpu_data(t_socket* socket_cpu, t_list* processes,
                          mtx_t* processes_mutex, int instruction_delay,
                          t_main_memory* main_memory, t_log* logger,
                          int* active_threads, mtx_t* active_threads_mutex,
                          cnd_t* active_threads_cond,
                          t_socket* socket_scheduler)
{
  t_cpu_data* cpu_data = malloc(sizeof(t_cpu_data));
  cpu_data->socket_cpu = socket_cpu;
  cpu_data->processes = processes;
  cpu_data->processes_mutex = processes_mutex;
  cpu_data->instruction_delay = instruction_delay;
  cpu_data->logger = logger;
  cpu_data->id = -1;
  cpu_data->main_memory = main_memory;
  cpu_data->active_threads = active_threads;
  cpu_data->active_threads_mutex = active_threads_mutex;
  cpu_data->active_threads_cond = active_threads_cond;
  cpu_data->socket_scheduler = socket_scheduler;
  return cpu_data;
}

t_stick_data* init_stick_data(t_socket* socket_stick, t_log* logger,
                              t_socket* socket_scheduler)
{
  t_stick_data* stick_data = malloc(sizeof(t_stick_data));
  stick_data->socket_stick = socket_stick;
  stick_data->logger = logger;
  stick_data->socket_scheduler = socket_scheduler;
  stick_data->stick_size = -1;
  stick_data->stick_port = -1;
  return stick_data;
}

t_swap_data* init_swap_data(t_socket* socket_swap, t_log* logger)
{
  t_swap_data* swap_data = malloc(sizeof(t_swap_data));
  swap_data->socket_swap = socket_swap;
  swap_data->logger = logger;
  int op = receive_op_code(socket_swap);
  if (op != OP_INFO_SWAP)
  {
    log_error(logger, "Unexpected opcode while receiving swap info");
    socket_destroy(socket_swap);
    free(swap_data);
    return NULL;
  }
  int a;
  t_swap_config* send_to_km = (t_swap_config*)receive_buffer(&a, socket_swap);
  swap_data->swap_size = send_to_km->swap_size;
  swap_data->block_size = send_to_km->block_size;
  free(send_to_km);
  swap_data->block_list = list_create();
  init_block_list(swap_data);
  return swap_data;
}

t_process* init_process(uint32_t pid, char* relative_path,
                        char* scripts_basepath, t_log* logger)
{
  t_process* process = malloc(sizeof(t_process));
  process->pid = pid;
  process->instructions_path = relative_path;
  process->segments = list_create();
  memset(&process->registers, 0,
         sizeof(t_registers));  // zero all register fields

  int length = strlen(scripts_basepath) + strlen(relative_path) + 2;
  char* full_path = malloc(length);
  snprintf(full_path, length, "%s/%s", scripts_basepath, relative_path);
  FILE* f = fopen(full_path, "r");
  if (f == NULL)
  {
    log_error(logger, "PID: %u - Could not open the file: %s", pid, full_path);
    free(full_path);
    free(process);
    return NULL;
  }
  process->instruction_count = count_instructions(f);
  process->instructions = malloc(sizeof(char*) * process->instruction_count);
  char line[256];
  int i = 0;
  while (fgets(line, sizeof(line), f))
  {
    line[strcspn(line, "\n")] = '\0';
    process->instructions[i++] = strdup(line);
  }
  fclose(f);
  free(full_path);
  return process;
}

bool resolve_stick_ip(t_stick_data* stick_data, t_socket* client_socket)
{
  if (socket_get_peer_ip(client_socket, stick_data->ip_memory_stick,
                         sizeof(stick_data->ip_memory_stick)))
    return true;
  log_error(stick_data->logger, "Could not resolve the memory stick IP");
  return false;
}

t_main_memory* init_main_memory(int max_segment_size,
                                t_allocation_strategy allocation_strategy,
                                int compaction_delay)
{
  t_main_memory* memory = malloc(sizeof(t_main_memory));
  memory->total_size = 0;
  memory->max_segment_size = max_segment_size;
  memory->compaction_delay = compaction_delay;
  memory->main_memory_mutex = malloc(sizeof(mtx_t));
  mtx_init(memory->main_memory_mutex, mtx_plain);
  memory->segments = list_create();
  memory->allocation_strategy = allocation_strategy;
  memory->holes = list_create();
  return memory;
}

static void init_block_list(t_swap_data* swap_data)
{
  int block_count = swap_data->swap_size / swap_data->block_size;
  for (int i = 0; i < block_count; i++)
  {
    t_block_data* block = malloc(sizeof(t_block_data));
    block->block_number = i;
    block->pid = -1;
    block->segment_number = -1;
    block->segment_block_number = -1;
    block->segment_size = -1;
    list_add(swap_data->block_list, block);
  }
}

static int count_instructions(FILE* f)
{
  int counter = 0;
  char line[256];
  while (fgets(line, sizeof(line), f))
    counter++;
  rewind(f);
  return counter;
}

#include "kernel_memory/cpu_listener.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/holes.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/segments.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/* listen_cpu's per-op handlers are static, so the only way to reach them is
 * through the real dispatch loop over a real socket, acting as the fake CPU
 * peer. Each test spins the loop up on its own thread, drives it through one
 * or more requests, then ends it and joins deterministically. Unlike
 * listen_scheduler, listen_cpu does NOT free its own cpu_data on the way
 * out (only closes its socket), so the fixture frees it after joining. */
typedef struct
{
  t_log* logger;
  int cpu_peer_fd;
  int scheduler_peer_fd;
  t_list* processes;
  pthread_mutex_t* processes_mutex;
  t_main_memory* memory;
  /* Heap-allocated, not an inline struct member: start_fixture returns
   * t_listener_fixture by value, so an inline int's address would point at
   * start_fixture's own stack frame, dangling the moment it returns. */
  int* active_threads;
  pthread_mutex_t* active_threads_mutex;
  pthread_cond_t* active_threads_cond;
  t_cpu_data* cpu_data;
  pthread_t thread;
} t_listener_fixture;

static t_listener_fixture start_fixture(void)
{
  t_listener_fixture f = {0};
  f.logger = km_quiet_logger();
  int cpu_server_fd;
  f.cpu_peer_fd = km_connected_pair(&cpu_server_fd);
  int scheduler_server_fd;
  f.scheduler_peer_fd = km_connected_pair(&scheduler_server_fd);

  f.processes = list_create();
  f.processes_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(f.processes_mutex, NULL);

  f.memory = init_main_memory(4096, AS_BEST, 0);

  f.active_threads = malloc(sizeof(int));
  *f.active_threads = 1;
  f.active_threads_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(f.active_threads_mutex, NULL);
  f.active_threads_cond = malloc(sizeof(pthread_cond_t));
  pthread_cond_init(f.active_threads_cond, NULL);

  f.cpu_data =
      init_cpu_data(cpu_server_fd, f.processes, f.processes_mutex, 0, f.memory,
                    f.logger, f.active_threads, f.active_threads_mutex,
                    f.active_threads_cond, scheduler_server_fd);

  pthread_create(&f.thread, NULL, listen_cpu, f.cpu_data);
  return f;
}

/* Sends OP_STICK_DISCONNECTED, which listen_cpu's handler treats as a
 * terminal condition: it notifies the scheduler peer, then ends the loop. */
static void disconnect_stick_and_join(t_listener_fixture* f)
{
  int op_code = OP_STICK_DISCONNECTED;
  cr_assert_eq(send(f->cpu_peer_fd, &op_code, sizeof(op_code), 0),
               sizeof(op_code));
  cr_assert_eq(receive_op_code(f->scheduler_peer_fd), OP_MEMORY_CORRUPTED);
  free(receive_string(f->scheduler_peer_fd));
  pthread_join(f->thread, NULL);
}

static void destroy_fixture(t_listener_fixture* f)
{
  close(f->cpu_peer_fd);
  close(f->scheduler_peer_fd);
  list_destroy_and_destroy_elements(f->processes,
                                    (void (*)(void*))free_process);
  free(f->processes_mutex);
  free_main_memory(f->memory);
  free(f->active_threads);
  free(f->active_threads_mutex);
  free(f->active_threads_cond);
  free(f->cpu_data);
  log_destroy(f->logger);
}

static t_process* add_process(t_listener_fixture* f, uint32_t pid)
{
  t_process* process = malloc(sizeof(t_process));
  process->pid = pid;
  process->instructions_path = NULL;
  process->instruction_count = 2;
  process->instructions = malloc(sizeof(char*) * 2);
  process->instructions[0] = strdup("NOOP");
  process->instructions[1] = strdup("EXIT");
  process->segments = list_create();
  memset(&process->registers, 0, sizeof(t_registers));
  list_add(f->processes, process);
  return process;
}

/* ── loop termination paths ───────────────────────────────────────────────*/

Test(km_cpu_listener,
     stick_disconnected_notifies_the_scheduler_and_ends_the_loop)
{
  t_listener_fixture f = start_fixture();
  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

Test(km_cpu_listener, closing_the_socket_ends_the_loop)
{
  t_listener_fixture f = start_fixture();
  close(f.cpu_peer_fd);
  pthread_join(f.thread, NULL);
  f.cpu_peer_fd = -1;
  destroy_fixture(&f);
}

Test(km_cpu_listener, an_unrecognized_op_code_ends_the_loop)
{
  t_listener_fixture f = start_fixture();
  int op_code = 99999;
  cr_assert_eq(send(f.cpu_peer_fd, &op_code, sizeof(op_code), 0),
               sizeof(op_code));
  pthread_join(f.thread, NULL);
  destroy_fixture(&f);
}

/* ── OP_NEXT_INSTRUCTION ─────────────────────────────────────────────────*/

Test(km_cpu_listener, next_instruction_sends_the_instruction_at_the_pc)
{
  t_listener_fixture f = start_fixture();
  add_process(&f, 1);

  t_packet* packet = create_packet(OP_NEXT_INSTRUCTION);
  uint32_t pid = 1, pc = 1;
  packet_append(packet, &pid, sizeof(pid));
  packet_append(packet, &pc, sizeof(pc));
  cr_assert(send_packet(packet, f.cpu_peer_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(f.cpu_peer_fd), OP_SEND_INSTRUCTION);
  char* instruction = receive_string(f.cpu_peer_fd);
  cr_assert_str_eq(instruction, "EXIT");
  free(instruction);

  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_REQUEST_CONTEXT ───────────────────────────────────────────────────*/

Test(km_cpu_listener, request_context_sends_registers_and_segment_table)
{
  t_listener_fixture f = start_fixture();
  t_process* process = add_process(&f, 2);
  process->registers.AX = 5;
  list_add(f.memory->segments, km_make_segment(0, 2, 0, 100));

  uint32_t pid = 2;
  cr_assert(send_buffer(OP_REQUEST_CONTEXT, &pid, sizeof(pid), f.cpu_peer_fd));

  cr_assert_eq(receive_op_code(f.cpu_peer_fd), OP_SEND_CONTEXT);
  int a;
  t_registers* registers = receive_buffer(&a, f.cpu_peer_fd);
  cr_assert_eq(registers->AX, 5);
  free(registers);

  cr_assert_eq(receive_op_code(f.cpu_peer_fd), OP_SEGMENT_TABLE);
  free(receive_packet(f.cpu_peer_fd));

  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

Test(km_cpu_listener, request_context_sends_nothing_for_an_unknown_pid)
{
  t_listener_fixture f = start_fixture();
  uint32_t pid = 404;
  cr_assert(send_buffer(OP_REQUEST_CONTEXT, &pid, sizeof(pid), f.cpu_peer_fd));

  /* no reply is sent for this pid; proceed straight to a request that does
   * reply, proving the loop kept running instead of hanging or exiting */
  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_UPDATED_CONTEXT ───────────────────────────────────────────────────*/

Test(km_cpu_listener, updated_context_stores_the_new_registers)
{
  t_listener_fixture f = start_fixture();
  t_process* process = add_process(&f, 3);

  t_packet* packet = create_packet(OP_UPDATED_CONTEXT);
  uint32_t pid = 3;
  t_registers registers = {0};
  registers.AX = 9;
  packet_append(packet, &pid, sizeof(pid));
  packet_append(packet, &registers, sizeof(registers));
  cr_assert(send_packet(packet, f.cpu_peer_fd));
  destroy_packet(packet);

  disconnect_stick_and_join(&f);
  cr_assert_eq(process->registers.AX, 9);
  destroy_fixture(&f);
}

Test(km_cpu_listener, updated_context_tolerates_an_unknown_pid)
{
  t_listener_fixture f = start_fixture();
  t_packet* packet = create_packet(OP_UPDATED_CONTEXT);
  uint32_t pid = 404;
  t_registers registers = {0};
  packet_append(packet, &pid, sizeof(pid));
  packet_append(packet, &registers, sizeof(registers));
  cr_assert(send_packet(packet, f.cpu_peer_fd));
  destroy_packet(packet);

  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_UPDATED_SEGMENT_TABLE ─────────────────────────────────────────────*/

Test(km_cpu_listener, updated_segment_table_sends_the_table)
{
  t_listener_fixture f = start_fixture();
  add_process(&f, 4);
  list_add(f.memory->segments, km_make_segment(0, 4, 0, 50));

  uint32_t pid = 4;
  cr_assert(
      send_buffer(OP_UPDATED_SEGMENT_TABLE, &pid, sizeof(pid), f.cpu_peer_fd));

  cr_assert_eq(receive_op_code(f.cpu_peer_fd), OP_SEGMENT_TABLE);
  free(receive_packet(f.cpu_peer_fd));

  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

Test(km_cpu_listener, updated_segment_table_sends_nothing_for_an_unknown_pid)
{
  t_listener_fixture f = start_fixture();
  uint32_t pid = 404;
  cr_assert(
      send_buffer(OP_UPDATED_SEGMENT_TABLE, &pid, sizeof(pid), f.cpu_peer_fd));

  disconnect_stick_and_join(&f);
  destroy_fixture(&f);
}

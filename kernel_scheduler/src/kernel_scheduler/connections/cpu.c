#include "kernel_scheduler/connections/cpu.h"

#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/common/handshake.h"
#include "kernel_scheduler/common/time.h"
#include "kernel_scheduler/syscalls/memory.h"
#include "utils/collections/list.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

const char* const PREEMPTION_REASONS[13] = {
    "no preemption occurred",
    "preemption by quantum end",
    "preemption by process priority",
    "preemption by compaction",
    "process termination",
    "first CPU cycle",
    "IO operation",
    "mutex blocked",
    "there is not enough memory for this instruction",
    "segmentation fault",
    "a mutex with that name already exists",
    "no mutex with that name exists",
    "this process cannot unlock this mutex"};

const char* const SYSCALL_NAMES[10] = {
    "MUTEX_CREATE", "MUTEX_LOCK", "MUTEX_UNLOCK", "MEM_ALLOC", "MEM_FREE",
    "SLEEP",        "STDOUT",     "STDIN",        "INIT_PROC", "EXIT"};

static void log_syscall(t_cpu_thread* data, int op_code);
static void close_thread_cpu(t_cpu_thread* data);
static void log_preemption_queue_priority(t_log* logger, uint32_t preempted_pid,
                                          int preempted_priority,
                                          uint32_t pid_new, int priority_new);
static void manage_queue_blocked(t_cpu_thread* data);
static void log_preemption_end_quantum(t_log* logger, uint32_t pid);
static void manage_preemption_priority(t_cpu_thread* data);
static void manage_end_quantum(t_cpu_thread* data);
static bool send_preemption(t_cpu_thread* data);
static void manage_request_process(t_cpu_thread* data);
static bool send_code(t_cpu_thread* data);
static void handle_cycle_cpu_ok(t_cpu_thread* data);
static void handle_segmentation_fault(t_cpu_thread* data);
static void handle_syscall_mutex_create(t_cpu_thread* data);
static void handle_syscall_mutex_lock(t_cpu_thread* data);
static void handle_syscall_mutex_unlock(t_cpu_thread* data);
static void handle_syscall_memory_allocation(t_cpu_thread* data);
static void handle_syscall_memory_free(t_cpu_thread* data);
static void handle_syscall_io_sleep(t_cpu_thread* data);
static void handle_syscall_io_stdout(t_cpu_thread* data);
static void handle_syscall_io_stdin(t_cpu_thread* data);
static void handle_syscall_start_process(t_cpu_thread* data);
static void handle_syscall_exit(t_cpu_thread* data);
static void handle_invalid_syscall(t_cpu_thread* data);
static bool send_pid(t_cpu_thread* data);
static void* handle_cpu_client(void* data_thread_cpu_void);
static void iterator_shutdown(void* value);
static t_cpu_thread* init_data_thread_cpu(
    int socket_cpu, t_list* list_sockets_cpu,
    pthread_mutex_t* mutex_list_sockets_cpu, pthread_cond_t* cpu_done_cond,
    char* id_cpu, t_log* logger, t_mutex_list* mutex_list, t_queues* queues,
    t_io* io, t_kernel_memory_socket* km_socket);
static bool create_thread_cpu(t_cpu_thread* data);
static char* get_id_cpu(int socket_cpu, t_log* logger);

bool handle_new_cpu(int socket_cpu, t_list* list_sockets_cpu,
                    pthread_mutex_t* mutex_list_sockets_cpu,
                    pthread_cond_t* cpu_done_cond, t_log* logger,
                    t_mutex_list* mutex_list, t_queues* queues, t_io* io,
                    t_kernel_memory_socket* km_socket)
{
  // Handshake with CPU
  if (!respond_handshake(socket_cpu, MID_KERNEL_SCHEDULER, logger))
    return false;

  // Get ID
  char* id_cpu = get_id_cpu(socket_cpu, logger);
  if (id_cpu == NULL)
    return false;

  // Initialize cpu thread data
  t_cpu_thread* data_thread_cpu = init_data_thread_cpu(
      socket_cpu, list_sockets_cpu, mutex_list_sockets_cpu, cpu_done_cond,
      id_cpu, logger, mutex_list, queues, io, km_socket);

  // Add socket to the list
  pthread_mutex_lock(mutex_list_sockets_cpu);
  list_add(list_sockets_cpu, &(data_thread_cpu->socket_fd));
  pthread_mutex_unlock(mutex_list_sockets_cpu);

  // Create thread
  if (!create_thread_cpu(data_thread_cpu))
  {
    pthread_mutex_lock(mutex_list_sockets_cpu);
    list_remove_element(list_sockets_cpu, &(data_thread_cpu->socket_fd));
    pthread_mutex_unlock(mutex_list_sockets_cpu);
    free(data_thread_cpu->id);
    free(data_thread_cpu);
    return false;
  }

  return true;
}

void close_cpu(t_list* list_sockets_cpu,
               pthread_mutex_t* mutex_list_sockets_cpu,
               pthread_cond_t* cpu_done_cond, t_queues* queues)
{
  terminate_queue_ready(&(queues->ready));
  pthread_mutex_lock(mutex_list_sockets_cpu);
  list_iterate(list_sockets_cpu, (void*)iterator_shutdown);
  while (!list_is_empty(list_sockets_cpu))
    pthread_cond_wait(cpu_done_cond, mutex_list_sockets_cpu);
  pthread_mutex_unlock(mutex_list_sockets_cpu);
  list_destroy(list_sockets_cpu);
  pthread_cond_destroy(cpu_done_cond);
  pthread_mutex_destroy(mutex_list_sockets_cpu);
}

static void log_syscall(t_cpu_thread* data, int op_code)
{
  if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
  {
    log_info(data->logger, "%u - Requested syscall: %s", data->pcb->pid,
             SYSCALL_NAMES[op_code - OP_SYSCALL_MUTEX_CREATE]);
  }
}

static void close_thread_cpu(t_cpu_thread* data)
{
  close(data->socket_fd);
  pthread_mutex_lock(data->socket_list_mutex);
  list_remove_element(data->socket_list, &(data->socket_fd));
  if (list_is_empty(data->socket_list))
    pthread_cond_signal(data->done_cond);
  pthread_mutex_unlock(data->socket_list_mutex);
  free(data->id);
  free(data);
}

static void log_preemption_queue_priority(t_log* logger, uint32_t preempted_pid,
                                          int preempted_priority,
                                          uint32_t pid_new, int priority_new)
{
  log_info(logger,
           "%u Priority: %d - Preempted by a higher-priority queue "
           "by process %u with priority %d",
           preempted_pid, preempted_priority, pid_new, priority_new);
}

static void manage_queue_blocked(t_cpu_thread* data)
{
  if (data->preemption_reason == PR_NO_PREEMPTION &&
      is_queue_ready_blocked(&(data->queues->ready)))
  {
    log_debug(data->logger, "CPU %s: Running blocked processes", data->id);
    transition_exec_ready(data->pcb, data->queues);
    data->preemption_reason = PR_COMPACTION;
  }
}

static void log_preemption_end_quantum(t_log* logger, uint32_t pid)
{
  log_info(logger, "%u - Preempted due to quantum end", pid);
}

static void manage_preemption_priority(t_cpu_thread* data)
{
  if (data->preemption_reason != PR_NO_PREEMPTION ||
      !data->queues->exec.preemption || data->pcb == NULL)
  {
    return;
  }

  int preempted_priority = get_priority_pcb(data->pcb);

  if (!is_lowest_priority_in_exec(&(data->queues->exec), preempted_priority))
  {
    return;
  }

  pthread_mutex_lock(&(data->queues->ready.queue_mutex));
  int priority_new = data->queues->ready.highest_priority;

  if (preempted_priority <= priority_new)
  {
    pthread_mutex_unlock(&(data->queues->ready.queue_mutex));
    return;
  }

  log_debug(data->logger, "CPU %s: Preempting process due to queue priority",
            data->id);
  t_pcb* new_pcb = transition_take_ready_next_no_mutex(&(data->queues->ready));
  pthread_mutex_unlock(&(data->queues->ready.queue_mutex));

  if (new_pcb == NULL)
  {
    log_warning(data->logger, "Priority preemption failed");
    return;
  }

  log_preemption_queue_priority(data->logger, data->pcb->pid,
                                preempted_priority, new_pcb->pid, priority_new);
  transition_exec_ready(data->pcb, data->queues);
  transition_ready_exec(new_pcb, data->queues);
  transition_to_exec(new_pcb, &(data->queues->exec));
  data->preemption_reason = PR_HIGHER_PRIORITY_PROCESS;
  data->pcb = new_pcb;
  data->counter = millis();
}

static void manage_end_quantum(t_cpu_thread* data)
{
  if (data->preemption_reason == PR_NO_PREEMPTION &&
      !is_queue_ready_empty(&(data->queues->ready)) &&
      get_algorithm_ready_queue(&(data->queues->ready),
                                get_priority_pcb(data->pcb)) == AP_RR &&
      quantum_ended(&(data->queues->exec), data->counter))
  {
    log_debug(data->logger, "CPU %s: Preempting due to quantum end", data->id);
    log_preemption_end_quantum(data->logger, data->pcb->pid);
    transition_exec_ready(data->pcb, data->queues);
    data->preemption_reason = PR_QUANTUM_END;
  }
}

static bool send_preemption(t_cpu_thread* data)
{
  log_trace(data->logger, "CPU %s: Sending preemption message: %s", data->id,
            PREEMPTION_REASONS[data->preemption_reason]);
  return send_string(
      (data->preemption_reason != PR_NO_PREEMPTION ? OP_INTERRUPT
                                                   : OP_NO_INTERRUPT),
      (char*)PREEMPTION_REASONS[data->preemption_reason], data->socket_fd);
}

static void manage_request_process(t_cpu_thread* data)
{
  if (data->preemption_reason != PR_NO_PREEMPTION)
  {
    log_trace(data->logger, "CPU %s: Requesting new process", data->id);
    data->counter = millis();
    data->pcb = transition_take_ready_blocking(&(data->queues->ready));
    if (data->pcb != NULL)
    {
      log_debug(data->logger, "CPU %s: got process: %u", data->id,
                data->pcb->pid);
      transition_ready_exec(data->pcb, data->queues);
      transition_to_exec(data->pcb, &(data->queues->exec));
    }
  }
}

static bool send_code(t_cpu_thread* data)
{
  return send_buffer(OP_RESUME_PROCESS, &(data->pcb->pid), sizeof(uint32_t),
                     data->socket_fd);
}

static void handle_cycle_cpu_ok(t_cpu_thread* data)
{
  free(receive_string(data->socket_fd));
  log_trace(data->logger, "CPU %s: CPU cycle OK", data->id);
}

static void handle_segmentation_fault(t_cpu_thread* data)
{
  free(receive_string(data->socket_fd));
  transition_exec_exit(data->pcb, data->queues, PER_SEGMENTATION_FAULT);
  data->preemption_reason = PR_SEGMENTATION_FAULT;
}

static void handle_syscall_mutex_create(t_cpu_thread* data)
{
  char* id_mutex = receive_string(data->socket_fd);
  switch (create_and_add_mutex(data->mutex_list, id_mutex, true, data->queues))
  {
    case RM_MUTEX_CREATED:
      break;
    case RM_MUTEX_NAME_ALREADY_EXISTS:
      transition_exec_exit(data->pcb, data->queues,
                           PER_MUTEX_NAME_ALREADY_EXISTS);
      data->preemption_reason = PR_MUTEX_NAME_ALREADY_EXISTS;
      break;
  }
  free(id_mutex);
}

static void handle_syscall_mutex_lock(t_cpu_thread* data)
{
  char* id_mutex = receive_string(data->socket_fd);
  switch (list_mutex_lock(data->mutex_list, id_mutex, data->pcb))
  {
    case RM_MUTEX_NAME_NOT_FOUND:
      transition_exec_exit(data->pcb, data->queues, PER_MUTEX_NAME_NOT_FOUND);
      data->preemption_reason = PR_MUTEX_NAME_NOT_FOUND;
      break;
    case RM_MUTEX_LOCKED:
      break;
    case RM_WAITING_MUTEX:
      data->preemption_reason = PR_MUTEX_LOCKED;
      break;
  }
  free(id_mutex);
}

static void handle_syscall_mutex_unlock(t_cpu_thread* data)
{
  char* id_mutex = receive_string(data->socket_fd);
  switch (list_mutex_unlock(data->mutex_list, id_mutex, data->pcb))
  {
    case RM_MUTEX_NAME_NOT_FOUND:
      transition_exec_exit(data->pcb, data->queues, PER_MUTEX_NAME_NOT_FOUND);
      data->preemption_reason = PR_MUTEX_NAME_NOT_FOUND;
      break;
    case RM_MUTEX_UNLOCKED:
      break;
    case RM_PROCESS_HAS_NO_LOCKED_MUTEX:
      transition_exec_exit(data->pcb, data->queues,
                           PER_PROCESS_HAS_NO_LOCKED_MUTEX);
      data->preemption_reason = PR_PROCESS_HAS_NO_LOCKED_MUTEX;
      break;
  }
  free(id_mutex);
}

static void handle_syscall_memory_allocation(t_cpu_thread* data)
{
  int size;
  t_syscall_memory* request = receive_buffer(&size, data->socket_fd);
  if (!allocate_memory(request, data->queues))
  {
    transition_exec_exit(data->pcb, data->queues, PER_NOT_ENOUGH_MEMORY);
    data->preemption_reason = PR_NOT_ENOUGH_MEMORY;
  }
  free(request);
}

static void handle_syscall_memory_free(t_cpu_thread* data)
{
  int size;
  t_syscall_memory* request = receive_buffer(&size, data->socket_fd);
  if (!free_memory(request, data->queues))
  {
    data->keep_running = false;
  }
  free(request);
}

static void handle_syscall_io_sleep(t_cpu_thread* data)
{
  int size;
  t_sleep_request* request = receive_buffer(&size, data->socket_fd);
  data->preemption_reason = PR_IO;
  if (!enqueue_io_request(request, &(data->io[E_SLEEP]), data->pcb))
  {
    transition_exec_exit(data->pcb, data->queues, PER_IO_FAILURE);
  }
  else
  {
    transition_exec_block(data->pcb, data->queues);
  }
}

static void handle_syscall_io_stdout(t_cpu_thread* data)
{
  int size;
  t_stdout_request* request = receive_buffer(&size, data->socket_fd);
  data->preemption_reason = PR_IO;
  if (!enqueue_io_request(request, &(data->io[E_STDOUT]), data->pcb))
  {
    transition_exec_exit(data->pcb, data->queues, PER_IO_FAILURE);
  }
  else
  {
    transition_exec_block(data->pcb, data->queues);
  }
}

static void handle_syscall_io_stdin(t_cpu_thread* data)
{
  int size;
  t_stdin_request* request = receive_buffer(&size, data->socket_fd);
  data->preemption_reason = PR_IO;
  if (!enqueue_io_request(request, &(data->io[E_STDIN]), data->pcb))
  {
    transition_exec_exit(data->pcb, data->queues, PER_IO_FAILURE);
  }
  else
  {
    transition_exec_block(data->pcb, data->queues);
  }
}

static void handle_syscall_start_process(t_cpu_thread* data)
{
  t_list* list = receive_packet(data->socket_fd);
  transition_new_ready(data->queues, list_get(list, 0),
                       *(int*)list_get(list, 1));
  list_destroy_and_destroy_elements(list, free);
}

static void handle_syscall_exit(t_cpu_thread* data)
{
  free(receive_string(data->socket_fd));
  transition_exec_exit(data->pcb, data->queues, PER_EXIT_INSTRUCTION);
  data->preemption_reason = PR_PROCESS_END;
}

static void handle_invalid_syscall(t_cpu_thread* data)
{
  data->keep_running = false;
}

static bool send_pid(t_cpu_thread* data)
{
  if (data->preemption_reason == PR_NO_PREEMPTION)
  {
    return true;
  }

  if (!send_code(data))
  {
    log_warning(data->logger, "CPU %s: could not send the interrupt decision",
                data->id);
    data->keep_running = false;
    return false;
  }

  log_trace(data->logger, "CPU %s: interrupt decision sent", data->id);
  data->preemption_reason = PR_NO_PREEMPTION;

  return true;
}

static void* handle_cpu_client(void* data_thread_cpu_void)
{
  t_cpu_thread* data = (t_cpu_thread*)data_thread_cpu_void;
  void (*syscall_handlers[OP_SYSCALL_EXIT - OP_CPU_CYCLE_OK + 2])(
      t_cpu_thread*) = {
      handle_cycle_cpu_ok,          handle_segmentation_fault,
      handle_syscall_mutex_create,  handle_syscall_mutex_lock,
      handle_syscall_mutex_unlock,  handle_syscall_memory_allocation,
      handle_syscall_memory_free,   handle_syscall_io_sleep,
      handle_syscall_io_stdout,     handle_syscall_io_stdin,
      handle_syscall_start_process, handle_syscall_exit,
      handle_invalid_syscall};

  while (data->keep_running)
  {
    manage_request_process(data);

    if (data->pcb == NULL)
    {
      log_debug(data->logger, "CPU %s: Shutdown of CPU", data->id);
      data->keep_running = false;
      break;
    }

    if (!send_pid(data))
    {
      break;
    }

    int op_code = receive_op_code(data->socket_fd);
    log_trace(data->logger, "CPU %s: Operation received: %d", data->id,
              op_code);
    if (op_code < OP_CPU_CYCLE_OK || op_code > OP_SYSCALL_EXIT)
    {
      op_code = OP_SYSCALL_EXIT + 1;
    }

    if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
    {
      increment_syscall_counter(data->queues);
    }
    log_syscall(data, op_code);
    syscall_handlers[op_code - OP_CPU_CYCLE_OK](data);
    if (op_code >= OP_SYSCALL_MUTEX_CREATE && op_code <= OP_SYSCALL_EXIT)
    {
      decrement_syscall_counter(data->queues);
    }

    manage_queue_blocked(data);
    manage_end_quantum(data);
    manage_preemption_priority(data);

    if (!send_preemption(data))
    {
      log_warning(data->logger, "CPU %s: Error sending the preemption",
                  data->id);
      data->keep_running = false;
      break;
    }

    if (data->preemption_reason == PR_HIGHER_PRIORITY_PROCESS)
    {
      log_trace(data->logger,
                "CPU %s: Updating preemption reason (process priority)",
                data->id);
      if (!send_pid(data))
      {
        break;
      }
    }
  }

  log_debug(data->logger, "CPU %s: Closing thread", data->id);
  if (data->preemption_reason == PR_HIGHER_PRIORITY_PROCESS &&
      data->pcb != NULL)
  {
    log_trace(data->logger, "CPU %s: Saving running process", data->id);
    transition_exec_ready(data->pcb, data->queues);
  }
  // Release connection and remove socket from the list
  close_thread_cpu(data);
  return NULL;
}

static void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

static t_cpu_thread* init_data_thread_cpu(
    int socket_cpu, t_list* list_sockets_cpu,
    pthread_mutex_t* mutex_list_sockets_cpu, pthread_cond_t* cpu_done_cond,
    char* id_cpu, t_log* logger, t_mutex_list* mutex_list, t_queues* queues,
    t_io* io, t_kernel_memory_socket* km_socket)
{
  t_cpu_thread* data = malloc(sizeof(t_cpu_thread));
  data->socket_fd = socket_cpu;
  data->socket_list = list_sockets_cpu;
  data->socket_list_mutex = mutex_list_sockets_cpu;
  data->done_cond = cpu_done_cond;
  data->id = id_cpu;
  data->logger = logger;
  data->mutex_list = mutex_list;
  data->queues = queues;
  data->io = io;
  data->km_socket = km_socket;
  data->pcb = NULL;
  data->keep_running = true;
  data->counter = 0;
  data->preemption_reason = PR_FIRST_CYCLE;
  return data;
}

static bool create_thread_cpu(t_cpu_thread* data)
{
  pthread_t thread_cpu;
  if (pthread_create(&thread_cpu, NULL, handle_cpu_client, data) != 0)
  {
    log_error(data->logger, "Error creating the CPU thread");
    return false;
  }
  pthread_detach(thread_cpu);
  return true;
}

static char* get_id_cpu(int socket_cpu, t_log* logger)
{
  if (receive_op_code(socket_cpu) != OP_ID_CPU)
  {
    log_warning(logger, "Error receiving the CPU ID");
    return NULL;
  }
  char* id_cpu = receive_string(socket_cpu);
  log_info(logger, "CPU %s connected", id_cpu);
  return id_cpu;
}

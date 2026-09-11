#include "kernel_scheduler/scheduler/queues.h"

#include <limits.h>
#include <unistd.h>

#include "kernel_scheduler/common/time.h"
#include "kernel_scheduler/connections/kernel_memory.h"
#include "kernel_scheduler/scheduler/compaction.h"
#include "kernel_scheduler/scheduler/scheduler_internal.h"
#include "kernel_scheduler/scheduler/suspension.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/msg.h"

const char* const PROCESS_END_REASONS[9] = {
    "invalid priority",
    "instruction EXIT",
    "system shutdown",
    "io failure",
    "not enough memory available",
    "segmentation fault",
    "a mutex with that name already exists",
    "no mutex with that name exists",
    "this process cannot unlock this mutex"};

static void wait_counter_threads(t_queues* queues);
static void destroy_counter_threads(t_queues* queues);
static void destroy_counter_syscalls(t_queues* queues);
static void log_transition_to_exit(t_log* logger, uint32_t pid, int reason);
static void transition_to_exit(t_pcb* pcb, t_queues* queues, int reason);
static t_pcb* transition_take_new(char* instructions_file, int priority,
                                  t_queues* queues);
static void transition_block_ready_no_mutex(t_pcb* pcb, t_queues* queues);
static bool transition_any_exit(t_queues* queues, int state, int reason);
static bool notify_new_process(t_queues* queues, char* instructions_file,
                               uint32_t pid);

t_queues* init_queues(int algorithm, t_list* cmn_algorithms, int quantum,
                      bool preemption, int server_socket, t_log* logger,
                      t_kernel_memory_socket* km_socket, int suspension_timeout)
{
  t_queues* queues = malloc(sizeof(t_queues));
  init_ready_queue(&(queues->ready), algorithm, cmn_algorithms);
  init_exec_list(&(queues->exec), quantum, preemption);
  init_blocking_list(&(queues->block));
  init_blocking_list(&(queues->susp_block));
  init_blocking_list(&(queues->susp_ready));
  queues->process_counter =
      init_counter_processes(server_socket, logger, km_socket);
  pthread_mutex_init(&(queues->routine_mutex), NULL);
  pthread_cond_init(&(queues->routine_cond), NULL);
  queues->routine_active = false;
  queues->terminate_routines = false;
  queues->logger = logger;
  queues->km_socket = km_socket;
  queues->server_socket = server_socket;
  atomic_init(&(queues->compaction_active), false);
  atomic_init(&(queues->resume_active), false);
  start_threads_suspended(queues, suspension_timeout);
  return queues;
}

void destroy_queues(t_queues* queues)
{
  terminate_routines(queues);
  wait_counter_threads(queues);
  destroy_counter_threads(queues);
  destroy_counter_syscalls(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  destroy_ready_queue(&(queues->ready));
  destroy_exec_list(&(queues->exec));
  destroy_blocking_list(&(queues->block));
  destroy_blocking_list(&(queues->susp_block));
  destroy_blocking_list(&(queues->susp_ready));
  destroy_counter_processes(queues->process_counter);
  pthread_mutex_destroy(&(queues->routine_mutex));
  pthread_cond_destroy(&(queues->routine_cond));
  free(queues);
}

void update_priority(t_pcb* pcb, t_queues* queues)
{
  if (!queues->ready.multilevel_queue)
  {
    return;
  }

  pthread_mutex_lock(&(pcb->state_mutex));
  if (pcb->state == EST_READY)
  {
    transition_take_ready(pcb, &(queues->ready));
    transition_to_ready(pcb, &(queues->ready));
  }
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_ready_exec(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  manage_state_pcb(queues->logger, pcb, EST_READY, EST_EXEC);
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_new_ready(t_queues* queues, char* instructions_file,
                          int priority)
{
  log_debug(queues->logger, "Creating process with priority %d located at %s",
            priority, instructions_file);
  t_pcb* pcb = transition_take_new(instructions_file, priority, queues);
  if (pcb != NULL)
  {
    if (!check_priority_valid(pcb, &(queues->ready)))
    {
      log_transition_state(queues->logger, pcb->pid, EST_NEW, EST_EXIT);
      transition_to_exit(pcb, queues, PER_INVALID_PRIORITY);
    }
    else
    {
      pcb->state = EST_READY;
      log_transition_state(queues->logger, pcb->pid, EST_NEW, EST_READY);
      transition_to_ready(pcb, &(queues->ready));
    }
  }
}

void transition_exec_ready(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  if (manage_state_pcb(queues->logger, pcb, EST_EXEC, EST_READY))
  {
    transition_take_exec(pcb, &(queues->exec), queues->syscall_counter);
    transition_to_ready(pcb, &(queues->ready));
  }
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_exec_exit(t_pcb* pcb, t_queues* queues, int reason)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  if (manage_state_pcb(queues->logger, pcb, EST_EXEC, EST_EXIT))
  {
    transition_take_exec(pcb, &(queues->exec), queues->syscall_counter);
    pthread_mutex_unlock(&(pcb->state_mutex));
    transition_to_exit(pcb, queues, reason);
  }
  else
  {
    pthread_mutex_unlock(&(pcb->state_mutex));
  }
}

void transition_exec_block(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  if (manage_state_pcb(queues->logger, pcb, EST_EXEC, EST_BLOCK))
  {
    transition_take_exec(pcb, &(queues->exec), queues->syscall_counter);
    transition_to_block(pcb, &(queues->block));
  }
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_block_ready(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  transition_block_ready_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_block_susp_block(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_susp_block(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  if (manage_state_pcb(queues->logger, pcb, EST_SUSP_BLOCK, EST_BLOCK))
  {
    transition_take_susp_block(pcb, &(queues->susp_block));
    transition_to_block(pcb, &(queues->block));
  }
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void transition_susp_block_susp_ready(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  transition_susp_block_susp_ready_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));
}

bool transition_susp_ready(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  bool ret = transition_susp_ready_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));
  return ret;
}

void transition_unlock(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(pcb->state_mutex));
  if (pcb->state == EST_BLOCK)
  {
    transition_block_ready_no_mutex(pcb, queues);
  }
  else
  {
    transition_susp_block_susp_ready_no_mutex(pcb, queues);
  }
  pthread_mutex_unlock(&(pcb->state_mutex));
}

void clear_queues(t_queues* queues)
{
  for (int i = EST_READY; i < EST_EXIT; i++)
  {
    while (transition_any_exit(queues, i, PER_SYSTEM_SHUTDOWN))
    {
    }
  }
}

void increment_syscall_counter(t_queues* queues)
{
  counter_increment(queues->syscall_counter);
}

void decrement_syscall_counter(t_queues* queues)
{
  pthread_mutex_lock(&(queues->syscall_counter->counter_mutex));
  queues->syscall_counter->count--;
  pthread_mutex_unlock(&(queues->syscall_counter->counter_mutex));
}

void increment_thread_counter(t_queues* queues)
{
  counter_increment(queues->thread_counter);
}

void decrement_thread_counter(t_queues* queues)
{
  pthread_mutex_lock(&(queues->thread_counter->counter_mutex));
  queues->thread_counter->count--;
  if (queues->thread_counter->count <= 0)
  {
    pthread_cond_signal(&(queues->thread_counter->condition));
  }
  pthread_mutex_unlock(&(queues->thread_counter->counter_mutex));
}

static void wait_counter_threads(t_queues* queues)
{
  log_debug(queues->logger, "Waiting for all threads to finish");
  pthread_mutex_lock(&(queues->thread_counter->counter_mutex));
  while (queues->thread_counter->count > 0)
  {
    pthread_cond_wait(&(queues->thread_counter->condition),
                      &(queues->thread_counter->counter_mutex));
  }
  pthread_mutex_unlock(&(queues->thread_counter->counter_mutex));
  log_debug(queues->logger, "Threads finished");
}

static void destroy_counter_threads(t_queues* queues)
{
  destroy_counter(queues->thread_counter);
}

static void destroy_counter_syscalls(t_queues* queues)
{
  destroy_counter(queues->syscall_counter);
}

void log_transition_state(t_log* logger, uint32_t pid, int previous_state,
                          int state_new)
{
  log_info(logger, "%d moves from state %s to state %s", pid,
           STATE_NAMES[previous_state], STATE_NAMES[state_new]);
}

void log_invalid_state(t_log* logger, uint32_t pid, int state,
                       int expected_state, int next_state)
{
  log_error(logger,
            "%u Cannot move from state %s to state %s because it "
            "is in state %s",
            pid, STATE_NAMES[expected_state], STATE_NAMES[next_state],
            STATE_NAMES[state]);
}

bool manage_state_pcb(t_log* logger, t_pcb* pcb, int expected_state,
                      int next_state)
{
  if (pcb->state == expected_state)
  {
    log_transition_state(logger, pcb->pid, expected_state, next_state);
    pcb->state = next_state;
    return true;
  }
  log_invalid_state(logger, pcb->pid, pcb->state, expected_state, next_state);
  return false;
}

static void log_transition_to_exit(t_log* logger, uint32_t pid, int reason)
{
  log_info(logger, "%u finished execution with reason: %s", pid,
           PROCESS_END_REASONS[reason]);
}

static void transition_to_exit(t_pcb* pcb, t_queues* queues, int reason)
{
  wait_zero_active_instances(pcb);

  if (reason != PER_SYSTEM_SHUTDOWN && reason != PER_INVALID_PRIORITY)
  {
    bool run_resumption_routine = process_size(queues, pcb->pid) > 0;
    if (notify_terminate_process(queues->km_socket, pcb->pid,
                                 queues->server_socket, queues->logger) &&
        run_resumption_routine)
    {
      create_resumption_routine_thread(queues);
    }
  }
  log_transition_to_exit(queues->logger, pcb->pid, reason);
  destroy_pcb(pcb);
  decrement_process_count(queues->process_counter);
}

static t_pcb* transition_take_new(char* instructions_file, int priority,
                                  t_queues* queues)
{
  t_pcb* pcb = create_pcb(EST_NEW, priority);
  log_info(queues->logger, "%u Creating the process - State: NEW", pcb->pid);

  increment_process_count(queues->process_counter);
  if (!notify_new_process(queues, instructions_file, pcb->pid))
  {
    log_transition_state(queues->logger, pcb->pid, EST_NEW, EST_EXIT);
    transition_to_exit(pcb, queues, PER_SYSTEM_SHUTDOWN);

    return NULL;
  }
  return pcb;
}

static void transition_block_ready_no_mutex(t_pcb* pcb, t_queues* queues)
{
  if (manage_state_pcb(queues->logger, pcb, EST_BLOCK, EST_READY))
  {
    transition_take_block(pcb, &(queues->block));
    transition_to_ready(pcb, &(queues->ready));
  }
}

static bool transition_any_exit(t_queues* queues, int state, int reason)
{
  t_pcb* pcb = NULL;
  switch (state)
  {
    case EST_READY:
      pcb = transition_take_ready_next(&(queues->ready));
      break;
    case EST_EXEC:
      pcb = transition_take_exec_next(&(queues->exec));
      break;
    case EST_BLOCK:
      pcb = transition_take_block_next(&(queues->block));
      break;
    case EST_SUSP_BLOCK:
      pcb = transition_take_susp_block_next(&(queues->susp_block));
      break;
    case EST_SUSP_READY:
      pcb = transition_take_susp_ready_next(&(queues->susp_ready));
      break;
  }

  if (pcb == NULL)
  {
    return false;
  }

  log_transition_state(queues->logger, pcb->pid, state, EST_EXIT);
  transition_to_exit(pcb, queues, reason);
  return true;
}

static bool notify_new_process(t_queues* queues, char* instructions_file,
                               uint32_t pid)
{
  t_packet* packet = create_packet(OP_NEW_PROCESS);
  packet_append_string(packet, instructions_file);
  packet_append(packet, &pid, sizeof(uint32_t));

  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  bool ret = send_packet(packet, queues->km_socket->km_socket);

  destroy_packet(packet);

  if (!ret)
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  ret = false;
  bool keep_running = true;
  while (keep_running)
  {
    int op_code = receive_op_code(queues->km_socket->km_socket);
    switch (op_code)
    {
      case OP_PROCESS_STARTED:
        free(receive_string(queues->km_socket->km_socket));
        ret = true;
        keep_running = false;
        break;
      case OP_MEMORY_CORRUPTED:
        free(receive_string(queues->km_socket->km_socket));
        close_kernel_scheduler(queues->server_socket, queues->logger,
                               SR_CORRUPTED_MEMORY, -1);
        keep_running = false;
        break;
      case OP_NEW_MEMORY_STICK:
        free(receive_string(queues->km_socket->km_socket));
        create_resumption_routine_thread(queues);
        keep_running = true;
        break;
      default:
        free(receive_string(queues->km_socket->km_socket));
        close_kernel_scheduler(queues->server_socket, queues->logger,
                               SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
        keep_running = false;
        break;
    }
  }

  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return ret;
}

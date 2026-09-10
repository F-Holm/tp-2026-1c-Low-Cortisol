#include "kernel_scheduler/common/time.h"
#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/shutdown.h"

#include <limits.h>
#include <unistd.h>

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

static void increment_thread_counter(t_queues* queues);
static void decrement_thread_counter(t_queues* queues);
static void wait_counter_threads(t_queues* queues);
static void destroy_counter_threads(t_queues* queues);
static void destroy_counter_syscalls(t_queues* queues);
static t_suspended_thread* init_data_thread_suspended(t_queues* queues);
static void init_data_thread_suspender(t_queues* queues,
                                       int suspension_timeout);
static void init_data_thread_resumer(t_queues* queues);
static void start_thread_suspender(t_queues* queues);
static void start_thread_resumer(t_queues* queues);
static void start_threads_suspended(t_queues* queues, int suspension_timeout);
static void terminate_thread_suspended(t_suspended_thread* data,
                                       t_blocking_list* list);
static void wait_thread_suspended(t_suspended_thread* data);
static void destroy_thread_suspended(t_suspended_thread* data);
static void destroy_thread_suspender(t_suspender_thread* data);
static void destroy_thread_resumer(t_resumer_thread* data);
static void terminate_threads_suspended(t_queues* queues);
static void destroy_threads_suspended(t_queues* queues);
static void terminate_routines(t_queues* queues);
static void log_transition_state(t_log* logger, uint32_t pid,
                                 int previous_state, int state_new);
static void log_invalid_state(t_log* logger, uint32_t pid, int state,
                              int expected_state, int next_state);
static bool manage_state_pcb(t_log* logger, t_pcb* pcb, int expected_state,
                             int next_state);
static void log_transition_to_exit(t_log* logger, uint32_t pid, int reason);
static void transition_to_exit(t_pcb* pcb, t_queues* queues, int reason);
static t_pcb* transition_take_new(char* instructions_file, int priority,
                                  t_queues* queues);
static void transition_block_ready_no_mutex(t_pcb* pcb, t_queues* queues);
static bool receive_suspend_process_response(t_queues* queues);
static bool notify_process_suspended(t_pcb* pcb, t_queues* queues);
static void transition_block_susp_block_no_mutex(t_pcb* pcb, t_queues* queues);
static void transition_susp_block_susp_ready_no_mutex(t_pcb* pcb,
                                                      t_queues* queues);
static bool notify_process_resume_suspended(t_pcb* pcb, t_queues* queues);
static bool can_resume_suspended(t_pcb* pcb, t_queues* queues);
static bool transition_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues);
static bool transition_any_exit(t_queues* queues, int state, int reason);
static void lock_thread_suspended(t_suspended_thread* data,
                                  t_blocking_list* list);
static void unlock_thread_suspended(t_suspended_thread* data);
static void wait_unlock(t_suspended_thread* data);
static t_pcb* get_process_blocked(t_queues* queues, t_suspender_thread* data);
static void run_suspend_process(t_queues* queues, t_suspender_thread* data,
                                t_pcb* process);
static void wait_process_blocked(t_queues* queues, t_suspender_thread* data);
static t_pcb* get_process_susp_ready(t_queues* queues, t_resumer_thread* data);
static void resume_suspended_process(t_queues* queues, t_resumer_thread* data,
                                     t_pcb* process);
static void wait_process_susp_ready(t_queues* queues, t_resumer_thread* data);
static void* thread_suspender(void* data_void);
static void* thread_resumer(void* data_void);
static void lock_total(t_queues* queues);
static bool fits_process(t_queues* queues, t_pcb* process);
static bool remove_of_the_list(t_queues* queues);
static void resumption_routine(t_queues* queues);
static void* resumption_routine_thread(void* data_resume_suspension);
static bool set_is_resuming(t_queues* queues, bool new_state);
static bool set_is_compacting(t_queues* queues, bool new_state);
static void routine_enter(t_queues* queues);
static void routine_leave(t_queues* queues);
static void* thread_unlock_queue_ready(void* args);
static void create_thread_unlock_queue_ready(t_queues* queues);
static bool compaction_finished(t_queues* queues);
static void compaction(t_queues* queues);
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

bool can_suspend(t_pcb* pcb, int suspension_timeout)
{
  return time_diff(millis(), pcb->blocked_time) >= suspension_timeout;
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

void lock_threads_suspended(t_queues* queues)
{
  lock_thread_suspended(queues->suspension_data->suspender_thread_data->data,
                        &(queues->block));
  lock_thread_suspended(queues->suspension_data->resumer_thread_data->data,
                        &(queues->susp_ready));
  log_trace(queues->logger, "Suspended threads locked");
}

void unlock_threads_suspended(t_queues* queues)
{
  unlock_thread_suspended(queues->suspension_data->suspender_thread_data->data);
  unlock_thread_suspended(queues->suspension_data->resumer_thread_data->data);
  log_trace(queues->logger, "Suspended threads unlocked");
}

// used for freed memory, a new stick or the end of compaction
void create_resumption_routine_thread(t_queues* queues)
{
  if (is_compacting(queues) || set_is_resuming(queues, true))
  {
    return;
  }

  pthread_t thread;
  if (pthread_create(&thread, NULL, resumption_routine_thread, queues) != 0)
  {
    log_error(queues->logger, "Error creating the resumption routine thread");
  }
  else
  {
    pthread_detach(thread);
    log_debug(queues->logger, "Resumption routine thread started successfully");
  }
}

void routine_compaction(t_queues* queues)
{
  if (set_is_compacting(queues, true))
  {
    return;
  }

  routine_enter(queues);
  if (!queues->terminate_routines)
  {
    lock_total(queues);
    compaction(queues);
    set_is_compacting(queues, false);
    create_thread_unlock_queue_ready(queues);
    create_resumption_routine_thread(queues);
  }
  routine_leave(queues);
}

bool is_compacting(t_queues* queues)
{
  return atomic_load(&(queues->compaction_active));
}

bool is_resuming(t_queues* queues)
{
  return atomic_load(&(queues->resume_active));
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

static void increment_thread_counter(t_queues* queues)
{
  counter_increment(queues->thread_counter);
}

static void decrement_thread_counter(t_queues* queues)
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

static t_suspended_thread* init_data_thread_suspended(t_queues* queues)
{
  t_suspended_thread* data = malloc(sizeof(t_suspended_thread));
  pthread_mutex_init(&(data->state_mutex), NULL);
  data->state = HS_RUNNING;
  pthread_cond_init(&(data->unlock), NULL);
  return data;
}

static void init_data_thread_suspender(t_queues* queues, int suspension_timeout)
{
  t_suspender_thread* data = malloc(sizeof(t_suspender_thread));
  data->data = init_data_thread_suspended(queues);
  data->data->wait_process = &(queues->block.new_process_cond);
  data->suspension_timeout = suspension_timeout;
  queues->suspension_data->suspender_thread_data = data;
}

static void init_data_thread_resumer(t_queues* queues)
{
  t_resumer_thread* data = malloc(sizeof(t_resumer_thread));
  data->data = init_data_thread_suspended(queues);
  data->data->wait_process = &(queues->susp_ready.new_process_cond);
  queues->suspension_data->resumer_thread_data = data;
}

static void start_thread_suspender(t_queues* queues)
{
  if (pthread_create(
          &(queues->suspension_data->suspender_thread_data->data->thread), NULL,
          thread_suspender, queues) != 0)
  {
    log_error(queues->logger, "Error creating the suspender thread");
  }
  else
  {
    log_debug(queues->logger, "Thread suspender started successfully");
  }
}

static void start_thread_resumer(t_queues* queues)
{
  if (pthread_create(
          &(queues->suspension_data->resumer_thread_data->data->thread), NULL,
          thread_resumer, queues) != 0)
  {
    log_error(queues->logger, "Error creating the resumer thread");
  }
  else
  {
    log_debug(queues->logger, "Thread resume started successfully");
  }
}

static void start_threads_suspended(t_queues* queues, int suspension_timeout)
{
  queues->suspension_data = malloc(sizeof(t_suspension_data));
  init_data_thread_suspender(queues, suspension_timeout);
  init_data_thread_resumer(queues);
  start_thread_suspender(queues);
  start_thread_resumer(queues);
}

static void terminate_thread_suspended(t_suspended_thread* data,
                                       t_blocking_list* list)
{
  pthread_mutex_lock(&(data->state_mutex));
  pthread_mutex_lock(&(list->list_mutex));
  pthread_cond_signal(data->wait_process);
  pthread_cond_signal(&(data->unlock));
  pthread_mutex_unlock(&(list->list_mutex));
  data->state = HS_FINISHING;
  pthread_mutex_unlock(&(data->state_mutex));
}

static void wait_thread_suspended(t_suspended_thread* data)
{
  pthread_join(data->thread, NULL);
  data->state = HS_FINISHED;
}

static void destroy_thread_suspended(t_suspended_thread* data)
{
  pthread_mutex_destroy(&(data->state_mutex));
  pthread_cond_destroy(&(data->unlock));
  free(data);
}

static void destroy_thread_suspender(t_suspender_thread* data)
{
  destroy_thread_suspended(data->data);
  free(data);
}

static void destroy_thread_resumer(t_resumer_thread* data)
{
  destroy_thread_suspended(data->data);
  free(data);
}

static void terminate_threads_suspended(t_queues* queues)
{
  terminate_thread_suspended(
      queues->suspension_data->suspender_thread_data->data, &(queues->block));
  terminate_thread_suspended(queues->suspension_data->resumer_thread_data->data,
                             &(queues->susp_ready));
  wait_thread_suspended(queues->suspension_data->suspender_thread_data->data);
  wait_thread_suspended(queues->suspension_data->resumer_thread_data->data);
}

static void destroy_threads_suspended(t_queues* queues)
{
  destroy_thread_suspender(queues->suspension_data->suspender_thread_data);
  destroy_thread_resumer(queues->suspension_data->resumer_thread_data);
  free(queues->suspension_data);
}

static void terminate_routines(t_queues* queues)
{
  pthread_mutex_lock(&(queues->routine_mutex));
  queues->terminate_routines = true;
  pthread_mutex_unlock(&(queues->routine_mutex));
}

static void log_transition_state(t_log* logger, uint32_t pid,
                                 int previous_state, int state_new)
{
  log_info(logger, "%d moves from state %s to state %s", pid,
           STATE_NAMES[previous_state], STATE_NAMES[state_new]);
}

static void log_invalid_state(t_log* logger, uint32_t pid, int state,
                              int expected_state, int next_state)
{
  log_error(logger,
            "%u Cannot move from state %s to state %s because it "
            "is in state %s",
            pid, STATE_NAMES[expected_state], STATE_NAMES[next_state],
            STATE_NAMES[state]);
}

static bool manage_state_pcb(t_log* logger, t_pcb* pcb, int expected_state,
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
  wait_0_instances_active_pcb(pcb);

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
  disminuir_counter_processes(queues->process_counter);
}

static t_pcb* transition_take_new(char* instructions_file, int priority,
                                  t_queues* queues)
{
  t_pcb* pcb = create_pcb(EST_NEW, priority);
  log_info(queues->logger, "%u Creating the process - State: NEW", pcb->pid);

  aumentar_counter_processes(queues->process_counter);
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

static bool receive_suspend_process_response(t_queues* queues)
{
  int op_code = receive_op_code(queues->km_socket->km_socket);
  switch (op_code)
  {
    case OP_SUSPENSION_OK:
      free(receive_string(queues->km_socket->km_socket));
      return true;
    case OP_SUSPENSION_FAILED:
      break;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return receive_suspend_process_response(queues);
    case OP_MEMORY_CORRUPTED:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      break;
    default:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      break;
  }
  free(receive_string(queues->km_socket->km_socket));
  return false;
}

static bool notify_process_suspended(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  if (!send_buffer(OP_SUSPEND_PROCESS, &(pcb->pid), sizeof(uint32_t),
                   queues->km_socket->km_socket))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  bool ret = receive_suspend_process_response(queues);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return ret;
}

static void transition_block_susp_block_no_mutex(t_pcb* pcb, t_queues* queues)
{
  if (pcb->state != EST_BLOCK)
  {
    log_invalid_state(queues->logger, pcb->pid, pcb->state, EST_BLOCK,
                      EST_SUSP_BLOCK);
    return;
  }

  if (!notify_process_suspended(pcb, queues))
  {
    log_debug(queues->logger, "Could not suspend process %u", pcb->pid);
    return;
  }

  log_transition_state(queues->logger, pcb->pid, EST_BLOCK, EST_SUSP_BLOCK);
  pcb->state = EST_SUSP_BLOCK;

  transition_take_block(pcb, &(queues->block));
  transition_to_susp_block(pcb, &(queues->susp_block));

  if (!atomic_load(&(queues->compaction_active)) &&
      !atomic_load(&(queues->resume_active)))
  {
    unlock_thread_suspended(queues->suspension_data->resumer_thread_data->data);
  }
}

static void transition_susp_block_susp_ready_no_mutex(t_pcb* pcb,
                                                      t_queues* queues)
{
  if (manage_state_pcb(queues->logger, pcb, EST_SUSP_BLOCK, EST_SUSP_READY))
  {
    transition_take_susp_block(pcb, &(queues->susp_block));
    transition_to_susp_ready(pcb, &(queues->susp_ready));
  }
}

static bool notify_process_resume_suspended(t_pcb* pcb, t_queues* queues)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));

  if (!(send_buffer(OP_RESUME_SUSPENDED_PROCESS, &(pcb->pid), sizeof(uint32_t),
                    queues->km_socket->km_socket)))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  bool ret = fits_process(queues, pcb);

  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));

  return ret;
}

static bool can_resume_suspended(t_pcb* pcb, t_queues* queues)
{
  return space_available(queues, pcb->pid) >= process_size(queues, pcb->pid);
}

static bool transition_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues)
{
  if (pcb->state != EST_SUSP_READY)
  {
    log_invalid_state(queues->logger, pcb->pid, pcb->state, EST_SUSP_READY,
                      EST_READY);
    return false;
  }

  if (!can_resume_suspended(pcb, queues))
  {
    log_debug(queues->logger, "Not enough space to resume process %u",
              pcb->pid);
    return false;
  }

  if (!notify_process_resume_suspended(pcb, queues))
  {
    log_debug(queues->logger, "Cannot resume process %u", pcb->pid);
    return false;
  }

  log_transition_state(queues->logger, pcb->pid, EST_SUSP_READY, EST_READY);
  pcb->state = EST_READY;
  transition_take_susp_ready(pcb, &(queues->susp_ready));
  transition_to_ready(pcb, &(queues->ready));
  return true;
}

static bool transition_any_exit(t_queues* queues, int state, int reason)
{
  t_pcb* pcb = NULL;
  switch (state)
  {
    case EST_READY:
      break;
    case EST_EXEC:
      break;
    case EST_BLOCK:
      break;
    case EST_SUSP_BLOCK:
      break;
    case EST_SUSP_READY:
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

static void lock_thread_suspended(t_suspended_thread* data,
                                  t_blocking_list* list)
{
  pthread_mutex_lock(&(data->state_mutex));
  switch (data->state)
  {
    case HS_RUNNING:
      data->state = HS_BLOCKED;
      break;
    case HS_WAITING_PROCESS:
      pthread_mutex_lock(&(list->list_mutex));
      data->state = HS_BLOCKED;
      pthread_cond_signal(data->wait_process);
      pthread_mutex_unlock(&(list->list_mutex));
      break;
  }
  pthread_mutex_unlock(&(data->state_mutex));
}

static void unlock_thread_suspended(t_suspended_thread* data)
{
  pthread_mutex_lock(&(data->state_mutex));
  if (data->state == HS_BLOCKED)
  {
    pthread_cond_signal(&(data->unlock));
    data->state = HS_RUNNING;
  }
  pthread_mutex_unlock(&(data->state_mutex));
}

static void wait_unlock(t_suspended_thread* data)
{
  while (data->state == HS_BLOCKED)
  {
    pthread_cond_wait(&(data->unlock), &(data->state_mutex));
  }
}

static t_pcb* get_process_blocked(t_queues* queues, t_suspender_thread* data)
{
  pthread_mutex_lock(&(queues->block.list_mutex));
  t_pcb* process = NULL;
  int size_in_memory = -1;
  t_list_iterator* iterador = list_iterator_create(queues->block.list);
  while (list_iterator_has_next(iterador))
  {
    process = list_iterator_next(iterador);
    size_in_memory = process_size_no_logger(queues, process->pid);
    if (size_in_memory > 0)
    {
      incrementar_instances_active_pcb(process);
      break;
    }
    else
    {
      process = NULL;
    }
  }
  if (process == NULL)
  {
    queues->block.new_process = false;
    data->data->state = HS_WAITING_PROCESS;
  }
  pthread_mutex_unlock(&(queues->block.list_mutex));
  list_iterator_destroy(iterador);
  return process;
}

static void run_suspend_process(t_queues* queues, t_suspender_thread* data,
                                t_pcb* process)
{
  pthread_mutex_unlock(&(data->data->state_mutex));
  pthread_mutex_lock(&(process->state_mutex));
  unsigned long sleep_time = millis() - process->blocked_time;
  bool must_suspend = sleep_time >= data->suspension_timeout;
  if (must_suspend && process->state == EST_BLOCK)
  {
    transition_block_susp_block_no_mutex(process, queues);
  }

  pthread_mutex_unlock(&(process->state_mutex));
  disminuir_instances_active_pcb(process);

  if (!must_suspend)
  {
    usleep(sleep_time > 500 ? 500 : sleep_time * 1000);
  }

  pthread_mutex_lock(&(data->data->state_mutex));
}

static void wait_process_blocked(t_queues* queues, t_suspender_thread* data)
{
  pthread_mutex_unlock(&(data->data->state_mutex));
  pthread_mutex_lock(&(queues->block.list_mutex));
  if (!queues->block.new_process)
  {
    pthread_cond_wait(data->data->wait_process, &(queues->block.list_mutex));
  }
  bool list_empty = list_is_empty(queues->block.list);
  pthread_mutex_unlock(&(queues->block.list_mutex));
  pthread_mutex_lock(&(data->data->state_mutex));
  if (!list_empty && data->data->state == HS_WAITING_PROCESS)
  {
    data->data->state = HS_RUNNING;
  }
}

static t_pcb* get_process_susp_ready(t_queues* queues, t_resumer_thread* data)
{
  pthread_mutex_lock(&(queues->susp_ready.list_mutex));
  t_pcb* process = NULL;
  if (!list_is_empty(queues->susp_ready.list))
  {
    process = list_get(queues->susp_ready.list, 0);
    incrementar_instances_active_pcb(process);
  }
  pthread_mutex_unlock(&(queues->susp_ready.list_mutex));
  if (process == NULL)
  {
    data->data->state = HS_WAITING_PROCESS;
  }
  return process;
}

static void resume_suspended_process(t_queues* queues, t_resumer_thread* data,
                                     t_pcb* process)
{
  pthread_mutex_unlock(&(data->data->state_mutex));
  pthread_mutex_lock(&(process->state_mutex));

  bool succeeded = false;
  if (process->state == EST_SUSP_READY)
  {
    succeeded = transition_susp_ready_no_mutex(process, queues);
  }

  pthread_mutex_unlock(&(process->state_mutex));
  disminuir_instances_active_pcb(process);

  if (!succeeded)
  {
    lock_thread_suspended(data->data, &(queues->susp_ready));
  }

  pthread_mutex_lock(&(data->data->state_mutex));
}

static void wait_process_susp_ready(t_queues* queues, t_resumer_thread* data)
{
  pthread_mutex_unlock(&(data->data->state_mutex));
  pthread_mutex_lock(&(queues->susp_ready.list_mutex));
  if (list_is_empty(queues->susp_ready.list))
  {
    pthread_cond_wait(data->data->wait_process,
                      &(queues->susp_ready.list_mutex));
  }
  bool list_empty = list_is_empty(queues->susp_ready.list);
  pthread_mutex_unlock(&(queues->susp_ready.list_mutex));
  pthread_mutex_lock(&(data->data->state_mutex));
  if (!list_empty && data->data->state == HS_WAITING_PROCESS)
  {
    data->data->state = HS_RUNNING;
  }
}

static void* thread_suspender(void* data_void)
{
  t_queues* queues = (t_queues*)data_void;
  t_suspender_thread* data = queues->suspension_data->suspender_thread_data;
  bool keep_running = true;

  while (keep_running)
  {
    pthread_mutex_lock(&(data->data->state_mutex));
    switch (data->data->state)
    {
      case HS_RUNNING:
        t_pcb* process = get_process_blocked(queues, data);
        if (process != NULL)
        {
          run_suspend_process(queues, data, process);
        }
        break;
      case HS_WAITING_PROCESS:
        wait_process_blocked(queues, data);
        break;
      case HS_BLOCKED:
        wait_unlock(data->data);
        break;
      case HS_FINISHING:
        keep_running = false;
        break;
    }
    pthread_mutex_unlock(&(data->data->state_mutex));
  }
  return NULL;
}

static void* thread_resumer(void* data_void)
{
  t_queues* queues = (t_queues*)data_void;
  t_resumer_thread* data = queues->suspension_data->resumer_thread_data;
  bool keep_running = true;

  while (keep_running)
  {
    pthread_mutex_lock(&(data->data->state_mutex));
    switch (data->data->state)
    {
      case HS_RUNNING:
        t_pcb* process = get_process_susp_ready(queues, data);
        if (process != NULL)
        {
          resume_suspended_process(queues, data, process);
        }
        break;
      case HS_WAITING_PROCESS:
        wait_process_susp_ready(queues, data);
        break;
      case HS_BLOCKED:
        wait_unlock(data->data);
        break;
      case HS_FINISHING:
        keep_running = false;
        break;
    }
    pthread_mutex_unlock(&(data->data->state_mutex));
  }
  return NULL;
}

// total lock/unlock functions
static void lock_total(t_queues* queues)
{
  lock_queue_ready(&(queues->ready));
  lock_threads_suspended(queues);
  wait_queue_exec_empty_with_syscalls(&(queues->exec), queues->syscall_counter);
}

static bool fits_process(t_queues* queues, t_pcb* process)
{
  int op_code = receive_op_code(queues->km_socket->km_socket);

  switch (op_code)
  {
    case OP_RESUME_SUSPENSION_FAILED:
      free(receive_string(queues->km_socket->km_socket));
      return false;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return fits_process(queues, process);
    case OP_MEMORY_CORRUPTED:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return false;
    case OP_RESUME_SUSPENSION_OK:
      free(receive_string(queues->km_socket->km_socket));
      return true;
    default:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return false;
  }
}

static bool remove_of_the_list(t_queues* queues)
{
  t_pcb* process = list_get(queues->susp_ready.list, 0);
  pthread_mutex_unlock(&(queues->susp_ready.list_mutex));
  pthread_mutex_lock(&(process->state_mutex));
  if (process->state == EST_SUSP_READY)
  {
    bool result = transition_susp_ready_no_mutex(process, queues);
    pthread_mutex_unlock(&(process->state_mutex));
    return result;
  }

  pthread_mutex_unlock(&(process->state_mutex));
  return true;
}

static void resumption_routine(t_queues* queues)
{
  bool keep_running = true;

  while (!blocking_list_is_empty(&(queues->susp_ready)) && keep_running)
  {
    if (is_compacting(queues))
    {
      pthread_mutex_unlock(&(queues->susp_ready.list_mutex));
      break;
    }

    keep_running = remove_of_the_list(queues);
  }
  pthread_mutex_unlock(&(queues->susp_ready.list_mutex));
}

static void* resumption_routine_thread(void* data_resume_suspension)
{
  t_queues* queues = (t_queues*)data_resume_suspension;
  increment_thread_counter(queues);
  routine_enter(queues);
  if (!queues->terminate_routines)
  {
    lock_threads_suspended(queues);
    resumption_routine(queues);
    set_is_resuming(queues, false);
    unlock_threads_suspended(queues);
    log_debug(queues->logger, "Resume-suspension routine ended");
  }
  routine_leave(queues);
  decrement_thread_counter(queues);
  return NULL;
}

static bool set_is_resuming(t_queues* queues, bool new_state)
{
  return atomic_exchange(&(queues->resume_active), new_state);
}

static bool set_is_compacting(t_queues* queues, bool new_state)
{
  return atomic_exchange(&(queues->compaction_active), new_state);
}

static void routine_enter(t_queues* queues)
{
  pthread_mutex_lock(&(queues->routine_mutex));
  while (queues->routine_active)
  {
    pthread_cond_wait(&(queues->routine_cond), &(queues->routine_mutex));
  }
  queues->routine_active = true;
  pthread_mutex_unlock(&(queues->routine_mutex));
}

static void routine_leave(t_queues* queues)
{
  pthread_mutex_lock(&(queues->routine_mutex));
  queues->routine_active = false;
  pthread_cond_signal(&(queues->routine_cond));
  pthread_mutex_unlock(&(queues->routine_mutex));
}

// COMPACTION ROUTINE
static void* thread_unlock_queue_ready(void* args)
{
  t_queues* queues = (t_queues*)args;
  increment_thread_counter(queues);

  wait_queue_exec_empty(&(queues->exec));

  if (!atomic_load(&(queues->compaction_active)))
  {
    unlock_queue_ready(&(queues->ready));
  }
  decrement_thread_counter(queues);
  return NULL;
}

static void create_thread_unlock_queue_ready(t_queues* queues)
{
  pthread_t thread;
  if (pthread_create(&thread, NULL, thread_unlock_queue_ready, queues) != 0)
  {
    log_error(queues->logger, "Error creating the ready-queue unblock thread");
  }
  else
  {
    pthread_detach(thread);
    log_debug(queues->logger,
              "Thread: ready-queue unblock started successfully");
  }
}

static bool compaction_finished(t_queues* queues)
{
  int op_code = -1;
  op_code = receive_op_code(queues->km_socket->km_socket);
  switch (op_code)
  {
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return compaction_finished(queues);
    case OP_COMPACTION_DONE:
      free(receive_string(queues->km_socket->km_socket));
      return true;
    case OP_MEMORY_CORRUPTED:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return false;
    default:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return false;
  }
}

static void compaction(t_queues* queues)
{
  if (!(send_string(OP_CAN_COMPACT, "Start compaction",
                    queues->km_socket->km_socket)))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    return;
  }
  log_info(queues->logger, "Start of compaction");
  if (compaction_finished(queues))
  {
    log_info(queues->logger, "End of compaction");
  }
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

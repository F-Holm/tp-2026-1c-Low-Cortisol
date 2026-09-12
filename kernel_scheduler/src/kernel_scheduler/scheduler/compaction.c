#include "kernel_scheduler/scheduler/compaction.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/exec_list.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "kernel_scheduler/scheduler/scheduler_internal.h"
#include "kernel_scheduler/scheduler/suspension.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/msg.h"

static void* resumption_routine_thread(void* data_resume_suspension);
static void resumption_routine(t_queues* queues);
static bool remove_of_the_list(t_queues* queues);
static bool set_is_resuming(t_queues* queues, bool new_state);
static bool set_is_compacting(t_queues* queues, bool new_state);
static void routine_enter(t_queues* queues);
static void routine_leave(t_queues* queues);
static void lock_total(t_queues* queues);
static void* thread_unlock_queue_ready(void* args);
static void create_thread_unlock_queue_ready(t_queues* queues);
static bool compaction_finished(t_queues* queues);
static void compaction(t_queues* queues);

bool is_compacting(t_queues* queues)
{
  return atomic_load(&(queues->compaction_active));
}

bool is_resuming(t_queues* queues)
{
  return atomic_load(&(queues->resume_active));
}

void terminate_routines(t_queues* queues)
{
  pthread_mutex_lock(&(queues->routine_mutex));
  queues->terminate_routines = true;
  pthread_mutex_unlock(&(queues->routine_mutex));
}

// used for freed memory, a new stick or the end of compaction
void create_resumption_routine_thread(t_queues* queues)
{
  if (is_compacting(queues) || set_is_resuming(queues, true))
  {
    return;
  }

  /* Incremented here, before the thread exists, rather than as the first
   * thing the new thread does: otherwise a caller that reaches
   * ks_wait_thread_counter_zero() right after pthread_create() returns can
   * observe the counter still at its old value and tear queues down out
   * from under a thread that hasn't run its first instruction yet. */
  increment_thread_counter(queues);
  pthread_t thread;
  if (pthread_create(&thread, NULL, resumption_routine_thread, queues) != 0)
  {
    log_error(queues->logger, "Error creating the resumption routine thread");
    decrement_thread_counter(queues);
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

// total lock/unlock functions
static void lock_total(t_queues* queues)
{
  lock_queue_ready(&(queues->ready));
  lock_threads_suspended(queues);
  wait_queue_exec_empty_with_syscalls(&(queues->exec), queues->syscall_counter);
}

bool fits_process(t_queues* queues, t_pcb* process)
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
  pthread_mutex_lock(&(queues->susp_ready.list_mutex));
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
      break;
    }

    keep_running = remove_of_the_list(queues);
  }
}

static void* resumption_routine_thread(void* data_resume_suspension)
{
  t_queues* queues = (t_queues*)data_resume_suspension;
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

static void* thread_unlock_queue_ready(void* args)
{
  t_queues* queues = (t_queues*)args;

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
  /* Incremented here, before the thread exists -- see the comment in
   * create_resumption_routine_thread() for why this can't be the first
   * thing the new thread itself does. */
  increment_thread_counter(queues);
  pthread_t thread;
  if (pthread_create(&thread, NULL, thread_unlock_queue_ready, queues) != 0)
  {
    log_error(queues->logger, "Error creating the ready-queue unblock thread");
    decrement_thread_counter(queues);
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

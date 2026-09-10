#include "kernel_scheduler/scheduler/suspension.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/common/time.h"
#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/compaction.h"
#include "kernel_scheduler/scheduler/memory_query.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "kernel_scheduler/scheduler/scheduler_internal.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

static void* thread_suspender(void* data_void);
static void* thread_resumer(void* data_void);
static t_suspended_thread* init_data_thread_suspended(t_queues* queues);
static void init_data_thread_suspender(t_queues* queues, int suspension_timeout);
static void init_data_thread_resumer(t_queues* queues);
static void start_thread_suspender(t_queues* queues);
static void start_thread_resumer(t_queues* queues);
static void terminate_thread_suspended(t_suspended_thread* data,
                                       t_blocking_list* list);
static void wait_thread_suspended(t_suspended_thread* data);
static void destroy_thread_suspended(t_suspended_thread* data);
static void destroy_thread_suspender(t_suspender_thread* data);
static void destroy_thread_resumer(t_resumer_thread* data);
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
static bool receive_suspend_process_response(t_queues* queues);
static bool notify_process_suspended(t_pcb* pcb, t_queues* queues);
static bool notify_process_resume_suspended(t_pcb* pcb, t_queues* queues);
static bool can_resume_suspended(t_pcb* pcb, t_queues* queues);

/* ── thread-data lifecycle ──────────────────────────────────────────────── */

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

void start_threads_suspended(t_queues* queues, int suspension_timeout)
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

void terminate_threads_suspended(t_queues* queues)
{
  terminate_thread_suspended(
      queues->suspension_data->suspender_thread_data->data, &(queues->block));
  terminate_thread_suspended(queues->suspension_data->resumer_thread_data->data,
                             &(queues->susp_ready));
  wait_thread_suspended(queues->suspension_data->suspender_thread_data->data);
  wait_thread_suspended(queues->suspension_data->resumer_thread_data->data);
}

void destroy_threads_suspended(t_queues* queues)
{
  destroy_thread_suspender(queues->suspension_data->suspender_thread_data);
  destroy_thread_resumer(queues->suspension_data->resumer_thread_data);
  free(queues->suspension_data);
}

/* ── park / wake ────────────────────────────────────────────────────────── */

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

/* ── Kernel Memory notifications ────────────────────────────────────────── */

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

/* ── state transitions ─────────────────────────────────────────────────── */

void transition_block_susp_block_no_mutex(t_pcb* pcb, t_queues* queues)
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

void transition_susp_block_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues)
{
  if (manage_state_pcb(queues->logger, pcb, EST_SUSP_BLOCK, EST_SUSP_READY))
  {
    transition_take_susp_block(pcb, &(queues->susp_block));
    transition_to_susp_ready(pcb, &(queues->susp_ready));
  }
}

bool transition_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues)
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

/* ── worker threads ────────────────────────────────────────────────────── */

static t_pcb* get_process_blocked(t_queues* queues, t_suspender_thread* data)
{
  pthread_mutex_lock(&(queues->block.list_mutex));
  t_pcb* process = NULL;
  int size_in_memory = -1;
  t_list_iterator* iterator = list_iterator_create(queues->block.list);
  while (list_iterator_has_next(iterator))
  {
    process = list_iterator_next(iterator);
    size_in_memory = process_size_no_logger(queues, process->pid);
    if (size_in_memory > 0)
    {
      increment_active_instances(process);
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
  list_iterator_destroy(iterator);
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
  decrement_active_instances(process);

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
    increment_active_instances(process);
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
  decrement_active_instances(process);

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

#include "kernel_scheduler/scheduler/ready_queue.h"

#include <limits.h>
#include <stdlib.h>

#include "utils/collections/list.h"

static void update_highest_priority_ready_no_mutex(t_ready_queue* ready);

void init_ready_queue(t_ready_queue* queue, int algorithm, t_list* cmn_algorithms)
{
  if (algorithm == AP_CMN)
  {
    queue->multilevel_queue = true;
    queue->queue_count = list_size(cmn_algorithms);
    queue->queues = malloc(queue->queue_count * sizeof(t_ready_subqueue));
    t_list_iterator* iterator_algorithms = list_iterator_create(cmn_algorithms);
    for (int i = 0; i < queue->queue_count; i++)
    {
      queue->queues[i].algorithm =
          *(int*)list_iterator_next(iterator_algorithms);
      queue->queues[i].queue = list_create();
    }
    list_iterator_destroy(iterator_algorithms);
  }
  else
  {
    queue->multilevel_queue = false;
    queue->queue_count = 1;
    queue->queues = malloc(sizeof(t_ready_subqueue));
    queue->queues->queue = list_create();
    queue->queues->algorithm = algorithm;
  }
  pthread_mutex_init(&(queue->queue_mutex), NULL);
  pthread_cond_init(&(queue->new_process), NULL);
  pthread_cond_init(&(queue->exit_unblocked), NULL);
  pthread_cond_init(&(queue->queue_empty), NULL);
  atomic_init(&(queue->preempt_all), false);
  queue->ready_process_count = 0;
  queue->highest_priority = INT_MAX;
  atomic_init(&(queue->terminate_queue), false);
}

void destroy_ready_queue(t_ready_queue* queue)
{
  for (int i = 0; i < queue->queue_count; i++)
  {
    list_destroy(queue->queues[i].queue);
  }
  pthread_cond_destroy(&(queue->new_process));
  pthread_cond_destroy(&(queue->exit_unblocked));
  pthread_mutex_destroy(&(queue->queue_mutex));
  pthread_cond_destroy(&(queue->queue_empty));
  free(queue->queues);
}

bool is_queue_ready_empty(t_ready_queue* ready)
{
  pthread_mutex_lock(&(ready->queue_mutex));
  bool ret = ready->ready_process_count == 0;
  pthread_mutex_unlock(&(ready->queue_mutex));
  return ret;
}

bool is_queue_ready_blocked(t_ready_queue* ready)
{
  return atomic_load(&(ready->preempt_all));
}

void lock_queue_ready(t_ready_queue* ready)
{
  atomic_store(&(ready->preempt_all), true);
}

void unlock_queue_ready(t_ready_queue* ready)
{
  atomic_store(&(ready->preempt_all), false);
  pthread_mutex_lock(&(ready->queue_mutex));
  pthread_cond_broadcast(&(ready->exit_unblocked));
  pthread_mutex_unlock(&(ready->queue_mutex));
}

bool queue_ready_terminated(t_ready_queue* ready)
{
  return atomic_load(&(ready->terminate_queue));
}

void terminate_queue_ready(t_ready_queue* ready)
{
  atomic_store(&(ready->terminate_queue), true);
  pthread_mutex_lock(&(ready->queue_mutex));
  pthread_cond_broadcast(&(ready->new_process));
  pthread_cond_broadcast(&(ready->exit_unblocked));
  pthread_mutex_unlock(&(ready->queue_mutex));
}

bool check_priority_valid(t_pcb* pcb, t_ready_queue* ready)
{
  return !ready->multilevel_queue || get_priority_pcb(pcb) < ready->queue_count;
}

void transition_to_ready(t_pcb* pcb, t_ready_queue* ready)
{
  pthread_mutex_lock(&(ready->queue_mutex));
  int priority = get_priority_pcb(pcb);
  if (ready->ready_process_count == 0 || ready->highest_priority > priority)
  {
    ready->highest_priority = priority;
  }

  if (ready->ready_process_count == 0)
  {
    pthread_cond_signal(&(ready->new_process));
  }
  ready->ready_process_count++;

  if (ready->multilevel_queue)
  {
    list_add(ready->queues[priority].queue, pcb);
  }
  else
  {
    list_add(ready->queues->queue, pcb);
  }
  pthread_mutex_unlock(&(ready->queue_mutex));
}

static void update_highest_priority_ready_no_mutex(t_ready_queue* ready)
{
  if (ready->ready_process_count > 0 && ready->multilevel_queue)
  {
    for (int i = 0; i < ready->queue_count; i++)
    {
      if (!list_is_empty(ready->queues[i].queue))
      {
        ready->highest_priority = i;
        return;
      }
    }
  }
  ready->highest_priority = ready->queue_count;
}

void transition_take_ready(t_pcb* pcb, t_ready_queue* ready)
{
  pthread_mutex_lock(&(ready->queue_mutex));
  int pos = ready->multilevel_queue ? get_priority_pcb(pcb) : 0;
  list_remove_element(ready->queues[pos].queue, pcb);
  update_highest_priority_ready_no_mutex(ready);
  ready->ready_process_count--;
  if (ready->ready_process_count == 0)
  {
    pthread_cond_signal(&(ready->queue_empty));
  }
  pthread_mutex_unlock(&(ready->queue_mutex));
}

t_pcb* transition_take_ready_next_no_mutex(t_ready_queue* ready)
{
  for (int i = 0; i < ready->queue_count; i++)
  {
    if (!list_is_empty(ready->queues[i].queue))
    {
      ready->ready_process_count--;
      t_pcb* pcb = list_remove(ready->queues[i].queue, 0);
      if (list_is_empty(ready->queues[i].queue))
      {
        update_highest_priority_ready_no_mutex(ready);
      }
      if (ready->ready_process_count == 0)
      {
        pthread_cond_signal(&(ready->queue_empty));
      }
      return pcb;
    }
  }
  return NULL;
}

t_pcb* transition_take_ready_next(t_ready_queue* ready)
{
  pthread_mutex_lock(&(ready->queue_mutex));
  t_pcb* pcb = transition_take_ready_next_no_mutex(ready);
  pthread_mutex_unlock(&(ready->queue_mutex));
  return pcb;
}

t_pcb* transition_take_ready_blocking(t_ready_queue* ready)
{
  pthread_mutex_lock(&(ready->queue_mutex));
  while (!queue_ready_terminated(ready) &&
         (ready->ready_process_count == 0 ||
          atomic_load(&(ready->preempt_all))))
  {
    if (!queue_ready_terminated(ready) && ready->ready_process_count == 0)
    {
      pthread_cond_wait(&(ready->new_process), &(ready->queue_mutex));
    }
    if (!queue_ready_terminated(ready) && atomic_load(&(ready->preempt_all)))
    {
      pthread_cond_wait(&(ready->exit_unblocked), &(ready->queue_mutex));
    }
  }

  t_pcb* pcb;
  if (queue_ready_terminated(ready))
  {
    pcb = NULL;
  }
  else
  {
    pcb = transition_take_ready_next_no_mutex(ready);
  }
  pthread_mutex_unlock(&(ready->queue_mutex));
  return pcb;
}

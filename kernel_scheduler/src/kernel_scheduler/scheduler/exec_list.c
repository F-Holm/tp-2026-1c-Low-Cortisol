#include "kernel_scheduler/scheduler/exec_list.h"

#include "utils/collections/list.h"

void init_exec_list(t_execute_list* list, int quantum, bool preemption)
{
  list->list = list_create();
  pthread_mutex_init(&(list->list_mutex), NULL);
  pthread_cond_init(&(list->queue_empty), NULL);
  list->lowest_priority = 0;
  list->quantum = quantum;
  list->preemption = preemption;
}

void destroy_exec_list(t_execute_list* list)
{
  list_destroy(list->list);
  pthread_mutex_destroy(&(list->list_mutex));
  pthread_cond_destroy(&(list->queue_empty));
}

void update_lowest_exec_priority(t_execute_list* exec)
{
  pthread_mutex_lock(&(exec->list_mutex));
  exec->lowest_priority = 0;
  t_list_iterator* iterator_list = list_iterator_create(exec->list);
  while (list_iterator_has_next(iterator_list))
  {
    t_pcb* pcb = list_iterator_next(iterator_list);

    int iterator_priority = get_priority_pcb(pcb);

    if (exec->lowest_priority < iterator_priority)
    {
      exec->lowest_priority = iterator_priority;
    }
  }
  list_iterator_destroy(iterator_list);
  pthread_mutex_unlock(&(exec->list_mutex));
}

void wait_queue_exec_empty(t_execute_list* exec)
{
  pthread_mutex_lock(&(exec->list_mutex));
  while (list_size(exec->list) > 0)
  {
    pthread_cond_wait(&(exec->queue_empty), &(exec->list_mutex));
  }
  pthread_mutex_unlock(&(exec->list_mutex));
}

void wait_queue_exec_empty_with_syscalls(t_execute_list* exec,
                                         t_counter* syscall_counter)
{
  pthread_mutex_lock(&(exec->list_mutex));
  pthread_mutex_lock(&(syscall_counter->counter_mutex));
  while (list_size(exec->list) - syscall_counter->count > 0)
  {
    pthread_cond_wait(&(syscall_counter->condition), &(exec->list_mutex));
  }
  pthread_mutex_unlock(&(syscall_counter->counter_mutex));
  pthread_mutex_unlock(&(exec->list_mutex));
}

void transition_to_exec(t_pcb* pcb, t_execute_list* exec)
{
  pthread_mutex_lock(&(exec->list_mutex));
  if (exec->preemption)
  {
    int priority_pcb = get_priority_pcb(pcb);
    if (exec->lowest_priority < priority_pcb)
    {
      exec->lowest_priority = priority_pcb;
    }
  }
  list_add(exec->list, pcb);
  pthread_mutex_unlock(&(exec->list_mutex));
}

void transition_take_exec(t_pcb* pcb, t_execute_list* exec,
                          t_counter* syscall_counter)
{
  pthread_mutex_lock(&(exec->list_mutex));
  list_remove_element(exec->list, pcb);
  if (list_size(exec->list) == 0)
  {
    pthread_cond_signal(&(exec->queue_empty));
  }
  pthread_mutex_lock(&(syscall_counter->counter_mutex));
  if (list_size(exec->list) - syscall_counter->count == 0)
  {
    pthread_cond_signal(&(syscall_counter->condition));
  }
  pthread_mutex_unlock(&(syscall_counter->counter_mutex));
  pthread_mutex_unlock(&(exec->list_mutex));
  if (exec->preemption)
  {
    update_lowest_exec_priority(exec);
  }
}

t_pcb* transition_take_exec_next(t_execute_list* exec)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(exec->list_mutex));
  if (!list_is_empty(exec->list))
  {
    pcb = list_remove(exec->list, 0);
  }
  pthread_mutex_unlock(&(exec->list_mutex));
  return pcb;
}

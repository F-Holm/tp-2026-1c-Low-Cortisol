#include "kernel_scheduler/scheduler/blocking_list.h"

#include <stdlib.h>

#include "kernel_scheduler/common/time.h"
#include "utils/collections/list.h"

static void set_blocked_time(t_pcb* pcb, unsigned long time)
{
  pcb->blocked_time = time;
}

void init_blocking_list(t_blocking_list* list)
{
  list->list = list_create();
  pthread_mutex_init(&(list->list_mutex), NULL);
  pthread_cond_init(&(list->new_process_cond), NULL);
  list->new_process = false;
}

void destroy_blocking_list(t_blocking_list* list)
{
  list_destroy(list->list);
  pthread_mutex_destroy(&(list->list_mutex));
  pthread_cond_destroy(&(list->new_process_cond));
}

bool blocking_list_is_empty(t_blocking_list* list)
{
  pthread_mutex_lock(&(list->list_mutex));
  bool empty = list_is_empty(list->list);
  pthread_mutex_unlock(&(list->list_mutex));
  return empty;
}

void transition_to_block(t_pcb* pcb, t_blocking_list* block)
{
  set_blocked_time(pcb, millis());
  pthread_mutex_lock(&(block->list_mutex));
  block->new_process = true;
  pthread_cond_signal(&(block->new_process_cond));
  list_add(block->list, pcb);
  pthread_mutex_unlock(&(block->list_mutex));
}

void transition_to_susp_block(t_pcb* pcb, t_blocking_list* susp_block)
{
  pthread_mutex_lock(&(susp_block->list_mutex));
  insert_pcb_sorted(susp_block->list, pcb);
  pthread_mutex_unlock(&(susp_block->list_mutex));
}

void transition_to_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->list_mutex));
  if (list_size(susp_ready->list) == 0)
  {
    pthread_cond_signal(&(susp_ready->new_process_cond));
  }
  insert_pcb_sorted(susp_ready->list, pcb);
  pthread_mutex_unlock(&(susp_ready->list_mutex));
}

void transition_take_block(t_pcb* pcb, t_blocking_list* block)
{
  pthread_mutex_lock(&(block->list_mutex));
  list_remove_element(block->list, pcb);
  pthread_mutex_unlock(&(block->list_mutex));
  set_blocked_time(pcb, 0);
}

t_pcb* transition_take_block_next(t_blocking_list* block)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(block->list_mutex));
  if (!list_is_empty(block->list))
  {
    pcb = list_remove(block->list, 0);
  }
  pthread_mutex_unlock(&(block->list_mutex));
  if (pcb != NULL)
  {
    set_blocked_time(pcb, 0);
  }
  return pcb;
}

void transition_take_susp_block(t_pcb* pcb, t_blocking_list* susp_block)
{
  pthread_mutex_lock(&(susp_block->list_mutex));
  list_remove_element(susp_block->list, pcb);
  pthread_mutex_unlock(&(susp_block->list_mutex));
}

t_pcb* transition_take_susp_block_next(t_blocking_list* susp_block)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(susp_block->list_mutex));
  if (!list_is_empty(susp_block->list))
  {
    pcb = list_remove(susp_block->list, 0);
  }
  pthread_mutex_unlock(&(susp_block->list_mutex));
  return pcb;
}

void transition_take_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->list_mutex));
  list_remove_element(susp_ready->list, pcb);
  pthread_mutex_unlock(&(susp_ready->list_mutex));
}

t_pcb* transition_take_susp_ready_next(t_blocking_list* susp_ready)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(susp_ready->list_mutex));
  if (!list_is_empty(susp_ready->list))
  {
    pcb = list_remove(susp_ready->list, 0);
  }
  pthread_mutex_unlock(&(susp_ready->list_mutex));
  return pcb;
}

#include "kernel_scheduler/domain/pcb.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "utils/mutex.h"

const char* const STATE_NAMES[7] = {
    "NEW", "READY", "EXEC", "BLOCK", "SUSP. BLOCK", "SUSP. READY", "EXIT"};

static bool is_highest_priority(void* pcb1, void* pcb2)
{
  return get_priority_pcb((t_pcb*)pcb1) <= get_priority_pcb((t_pcb*)pcb2);
}

t_pcb* create_pcb(int state, int priority)
{
  static atomic_uint pid = 0;
  t_pcb* pcb = malloc(sizeof(t_pcb));

  mtx_init(&(pcb->priority_mutex));
  mtx_init(&(pcb->state_mutex));
  mtx_init(&(pcb->active_instances_mutex));
  cnd_init(&(pcb->no_active_instances));
  pcb->active_instances = 0;
  pcb->blocked_time = 0;
  pcb->state = state;
  pcb->blocking_mutex = NULL;

  pcb->priority_list = list_create();
  pcb->priority = priority;
  int* aux = malloc(sizeof(int));
  *aux = priority;
  list_add(pcb->priority_list, aux);

  pcb->pid = atomic_fetch_add(&pid, 1);
  return pcb;
}

void destroy_pcb(t_pcb* pcb)
{
  mtx_destroy(&(pcb->priority_mutex));
  mtx_destroy(&(pcb->state_mutex));
  mtx_destroy(&(pcb->active_instances_mutex));
  cnd_destroy(&(pcb->no_active_instances));
  list_destroy_and_destroy_elements(pcb->priority_list, free);
  free(pcb);
}

int get_state_pcb(t_pcb* pcb)
{
  mtx_lock(&(pcb->state_mutex));
  int state_pcb = pcb->state;
  mtx_unlock(&(pcb->state_mutex));
  return state_pcb;
}

int get_priority_pcb(t_pcb* pcb)
{
  mtx_lock(&(pcb->priority_mutex));
  int priority_pcb = pcb->priority;
  mtx_unlock(&(pcb->priority_mutex));
  return priority_pcb;
}

int insert_pcb_sorted(t_list* list, t_pcb* pcb)
{
  return list_add_sorted(list, pcb, is_highest_priority);
}

void increment_active_instances(t_pcb* pcb)
{
  mtx_lock(&(pcb->active_instances_mutex));
  pcb->active_instances++;
  mtx_unlock(&(pcb->active_instances_mutex));
}

void decrement_active_instances(t_pcb* pcb)
{
  mtx_lock(&(pcb->active_instances_mutex));
  pcb->active_instances--;
  if (pcb->active_instances == 0)
  {
    cnd_signal(&(pcb->no_active_instances));
  }
  mtx_unlock(&(pcb->active_instances_mutex));
}

void wait_zero_active_instances(t_pcb* pcb)
{
  mtx_lock(&(pcb->active_instances_mutex));
  while (pcb->active_instances != 0)
  {
    cnd_wait(&(pcb->no_active_instances), &(pcb->active_instances_mutex));
  }
  mtx_unlock(&(pcb->active_instances_mutex));
}

void set_mutex_blocking(t_pcb* pcb, void* mutex)
{
  mtx_lock(&(pcb->priority_mutex));
  pcb->blocking_mutex = mutex;
  mtx_unlock(&(pcb->priority_mutex));
}

void* get_mutex_blocking(t_pcb* pcb)
{
  mtx_lock(&(pcb->priority_mutex));
  void* mutex = pcb->blocking_mutex;
  mtx_unlock(&(pcb->priority_mutex));
  return mutex;
}

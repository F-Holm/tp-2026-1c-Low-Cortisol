#include "kernel_scheduler/syscalls/mutex.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void log_mutex_tomado(t_log* logger, uint32_t pid, char* id_mutex);
static void log_mutex_released(t_log* logger, uint32_t pid, char* id_mutex);
static void log_transition_of_priority(t_log* logger, uint32_t pid,
                                       int previous_priority, int priority_new);
static void destroy_mutex_iterator(void* mutex);
static t_mutex* create_mutex(char* id, bool priority_active, t_queues* queues);
static void remove_priority_list(t_list* list, int priority);
static bool has_higher_priority_than(void* p1, void* p2);
static void insert_priority(t_pcb* pcb, int priority, t_log* logger);
static void replace_priority(t_pcb* pcb, int old_priority, int priority_new,
                             t_queues* queues);
static bool remove_pcb_list(t_list* list, t_pcb* pcb);
static void propagate_priority_transitive(t_pcb* pcb, int priority_new,
                                          t_queues* queues);
static bool remove_priority(t_pcb* pcb, int priority, t_log* logger);
static int mutex_lock(t_mutex* mutex, t_pcb* pcb);
static int mutex_unlock(t_mutex* mutex, t_pcb* pcb);
static void destroy_mutex(t_mutex* mutex);

t_mutex_list* init_list_mutex(void)
{
  t_mutex_list* mutex_list = malloc(sizeof(t_mutex_list));
  mutex_list->list = dictionary_create();
  pthread_mutex_init(&(mutex_list->list_mutex), NULL);
  return mutex_list;
}

void destroy_list_mutex(t_mutex_list* mutex_list)
{
  dictionary_destroy_and_destroy_elements(mutex_list->list,
                                          destroy_mutex_iterator);
  pthread_mutex_destroy(&(mutex_list->list_mutex));
  free(mutex_list);
}

int create_and_add_mutex(t_mutex_list* mutex_list, char* id,
                         bool priority_active, t_queues* queues)
{
  pthread_mutex_lock(&(mutex_list->list_mutex));
  bool ret = !dictionary_has_key(mutex_list->list, id);
  if (ret)
  {
    dictionary_put(mutex_list->list, id,
                   create_mutex(id, priority_active, queues));
  }
  pthread_mutex_unlock(&(mutex_list->list_mutex));

  return ret ? RM_MUTEX_CREATED : RM_MUTEX_NAME_ALREADY_EXISTS;
}

int list_mutex_lock(t_mutex_list* mutex_list, char* id, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex_list->list_mutex));
  t_mutex* mutex = dictionary_get(mutex_list->list, id);
  pthread_mutex_unlock(&(mutex_list->list_mutex));
  return mutex == NULL ? RM_MUTEX_NAME_NOT_FOUND : mutex_lock(mutex, pcb);
}

int list_mutex_unlock(t_mutex_list* mutex_list, char* id, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex_list->list_mutex));
  t_mutex* mutex = dictionary_get(mutex_list->list, id);
  pthread_mutex_unlock(&(mutex_list->list_mutex));
  return mutex == NULL ? RM_MUTEX_NAME_NOT_FOUND : mutex_unlock(mutex, pcb);
}

static void log_mutex_tomado(t_log* logger, uint32_t pid, char* id_mutex)
{
  log_info(logger, "%u Takes the Mutex %s", pid, id_mutex);
}

static void log_mutex_released(t_log* logger, uint32_t pid, char* id_mutex)
{
  log_info(logger, "%u Releases the Mutex %s", pid, id_mutex);
}

static void log_transition_of_priority(t_log* logger, uint32_t pid,
                                       int previous_priority, int priority_new)
{
  log_info(logger, "%u Change of priority: %d - %d", pid, previous_priority,
           priority_new);
}

static void destroy_mutex_iterator(void* mutex)
{
  destroy_mutex(mutex);
}

static t_mutex* create_mutex(char* id, bool priority_active, t_queues* queues)
{
  t_mutex* mutex = malloc(sizeof(t_mutex));
  mutex->id = malloc(strlen(id) + 1);
  strcpy(mutex->id, id);
  mutex->next_priority = INT_MAX;
  pthread_mutex_init(&(mutex->mutex), NULL);
  mutex->priority_active = priority_active;
  mutex->list = list_create();
  mutex->current_process = NULL;
  mutex->state = 1;
  mutex->queues = queues;
  return mutex;
}

static void remove_priority_list(t_list* list, int priority)
{
  t_list_iterator* iterador_list = list_iterator_create(list);
  while (list_iterator_has_next(iterador_list))
  {
    int* temp = (int*)list_iterator_next(iterador_list);
    if (*temp == priority)
    {
      list_iterator_remove(iterador_list);
      free(temp);
      break;
    }
  }
  list_iterator_destroy(iterador_list);
}

static bool has_higher_priority_than(void* p1, void* p2)
{
  return *(int*)p1 < *(int*)p2;
}

static void insert_priority(t_pcb* pcb, int priority, t_log* logger)
{
  int* aux = malloc(sizeof(int));
  *aux = priority;
  pthread_mutex_lock(&(pcb->priority_mutex));
  list_add_sorted(pcb->priority_list, aux, has_higher_priority_than);
  if (priority < pcb->priority)
  {
    log_transition_of_priority(logger, pcb->pid, pcb->priority, priority);
    pcb->priority = priority;
  }
  pthread_mutex_unlock(&(pcb->priority_mutex));
}

static void replace_priority(t_pcb* pcb, int old_priority, int priority_new,
                             t_queues* queues)
{
  bool act = false;
  int* aux = malloc(sizeof(int));
  *aux = priority_new;
  pthread_mutex_lock(&(pcb->priority_mutex));
  remove_priority_list(pcb->priority_list, old_priority);
  list_add_sorted(pcb->priority_list, aux, has_higher_priority_than);
  int priority = *(int*)list_get(pcb->priority_list, 0);
  if (priority != pcb->priority)
  {
    act = true;
    log_transition_of_priority(queues->logger, pcb->pid, pcb->priority,
                               priority);
    pcb->priority = priority;
  }
  pthread_mutex_unlock(&(pcb->priority_mutex));
  if (act)
  {
    update_priority(pcb, queues);
    propagate_priority_transitive(pcb, priority, queues);
  }
}

static bool remove_pcb_list(t_list* list, t_pcb* pcb)
{
  bool found = false;
  t_list_iterator* iterador_list = list_iterator_create(list);
  while (list_iterator_has_next(iterador_list))
  {
    if ((t_pcb*)list_iterator_next(iterador_list) == pcb)
    {
      list_iterator_remove(iterador_list);
      found = true;
      break;
    }
  }
  list_iterator_destroy(iterador_list);
  return found;
}

static void propagate_priority_transitive(t_pcb* pcb, int priority_new,
                                          t_queues* queues)
{
  t_mutex* expected_mutex = get_mutex_blocking(pcb);
  if (expected_mutex == NULL)
  {
    return;
  }

  pthread_mutex_lock(&(expected_mutex->mutex));

  if (!expected_mutex->priority_active)
  {
    pthread_mutex_unlock(&(expected_mutex->mutex));
    return;
  }

  if (!remove_pcb_list(expected_mutex->list, pcb))
  {
    pthread_mutex_unlock(&(expected_mutex->mutex));
    return;
  }

  bool now_is_highest_priority =
      insert_pcb_in_orden(expected_mutex->list, pcb) == 0;

  if (now_is_highest_priority && priority_new != expected_mutex->next_priority)
  {
    int previous_next_priority = expected_mutex->next_priority;
    expected_mutex->next_priority = priority_new;
    replace_priority(expected_mutex->current_process, previous_next_priority,
                     priority_new, queues);
  }

  pthread_mutex_unlock(&(expected_mutex->mutex));
}

static bool remove_priority(t_pcb* pcb, int priority, t_log* logger)
{
  pthread_mutex_lock(&(pcb->priority_mutex));
  remove_priority_list(pcb->priority_list, priority);

  int new_priority = *(int*)list_get(pcb->priority_list, 0);
  bool updated_priority = pcb->priority != new_priority;
  if (updated_priority)
  {
    log_transition_of_priority(logger, pcb->pid, pcb->priority, new_priority);
    pcb->priority = new_priority;
  }
  pthread_mutex_unlock(&(pcb->priority_mutex));

  return updated_priority;
}

static int mutex_lock(t_mutex* mutex, t_pcb* pcb)
{
  int ret = RM_WAITING_MUTEX;
  int priority_pcb = get_priority_pcb(pcb);
  pthread_mutex_lock(&(mutex->mutex));
  if (mutex->state == 1)
  {
    log_mutex_tomado(mutex->queues->logger, pcb->pid, mutex->id);
    mutex->current_process = pcb;
    mutex->next_priority = INT_MAX;
    insert_priority(pcb, mutex->next_priority, mutex->queues->logger);
    ret = RM_MUTEX_LOCKED;
  }
  else if (mutex->priority_active)
  {
    if (insert_pcb_in_orden(mutex->list, pcb) == 0)
    {
      replace_priority(mutex->current_process, mutex->next_priority,
                       priority_pcb, mutex->queues);
      mutex->next_priority = priority_pcb;
      update_lowest_exec_priority(&(mutex->queues->exec));
    }
    set_mutex_blocking(pcb, mutex);
    transition_exec_block(pcb, mutex->queues);
  }
  else
  {
    list_add(mutex->list, pcb);
    set_mutex_blocking(pcb, mutex);
    transition_exec_block(pcb, mutex->queues);
  }
  mutex->state--;
  pthread_mutex_unlock(&(mutex->mutex));
  return ret;
}

static int mutex_unlock(t_mutex* mutex, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex->mutex));
  if (pcb != mutex->current_process)
  {
    pthread_mutex_unlock(&(mutex->mutex));
    return RM_PROCESS_HAS_NO_LOCKED_MUTEX;
  }
  if (mutex->priority_active &&
      remove_priority(pcb, mutex->next_priority, mutex->queues->logger))
  {
    update_lowest_exec_priority(&(mutex->queues->exec));
  }
  log_mutex_released(mutex->queues->logger, pcb->pid, mutex->id);
  if (mutex->state == 0)
  {
    mutex->current_process = NULL;
    mutex->next_priority = INT_MAX;
  }
  else if (mutex->state < 0)
  {
    mutex->current_process = list_remove(mutex->list, 0);
    set_mutex_blocking(mutex->current_process, NULL);
    if (mutex->priority_active)
    {
      if (mutex->state == -1)
      {
        mutex->next_priority = INT_MAX;
      }
      else
      {
        mutex->next_priority = get_priority_pcb(list_get(mutex->list, 0));
      }
      insert_priority(mutex->current_process, mutex->next_priority,
                      mutex->queues->logger);
    }
    log_mutex_tomado(mutex->queues->logger, mutex->current_process->pid,
                     mutex->id);
    transition_unlock(mutex->current_process, mutex->queues);
  }
  mutex->state++;
  pthread_mutex_unlock(&(mutex->mutex));
  return RM_MUTEX_UNLOCKED;
}

static void destroy_mutex(t_mutex* mutex)
{
  pthread_mutex_lock(&(mutex->mutex));
  t_list_iterator* iterador_list = list_iterator_create(mutex->list);
  while (list_iterator_has_next(iterador_list))
  {
    t_pcb* pcb = list_iterator_next(iterador_list);
    list_iterator_remove(iterador_list);
    set_mutex_blocking(pcb, NULL);
    transition_unlock(pcb, mutex->queues);
  }
  list_iterator_destroy(iterador_list);
  free(mutex->id);
  pthread_mutex_unlock(&(mutex->mutex));
  pthread_mutex_destroy(&(mutex->mutex));
  list_destroy(mutex->list);
  free(mutex);
}

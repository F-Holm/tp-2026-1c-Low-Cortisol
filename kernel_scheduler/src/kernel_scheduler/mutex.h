#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/queue.h"
#include "utils/collections/dictionary.h"
#include "utils/collections/list.h"
#include "utils/log.h"

typedef struct
{
  char* id;
  int next_priority;
  pthread_mutex_t mutex;
  bool priority_active;
  t_list* list;
  t_pcb* current_process;
  int state;
  t_queues* queues;
} t_mutex;

typedef struct
{
  t_dictionary* list;
  pthread_mutex_t list_mutex;
} t_mutex_list;

typedef enum
{
  RM_MUTEX_CREATED,
  RM_MUTEX_NAME_ALREADY_EXISTS,
  RM_MUTEX_NAME_NOT_FOUND,
  RM_MUTEX_LOCKED,
  RM_WAITING_MUTEX,
  RM_MUTEX_UNLOCKED,
  RM_PROCESS_HAS_NO_LOCKED_MUTEX
} t_mutex_result;

t_mutex_list* init_list_mutex(void);
void destroy_list_mutex(t_mutex_list* mutex_list);

int create_and_add_mutex(t_mutex_list* mutex_list, char* id,
                         bool priority_active, t_queues* queues);
int list_mutex_lock(t_mutex_list* mutex_list, char* id, t_pcb* pcb);
int list_mutex_unlock(t_mutex_list* mutex_list, char* id, t_pcb* pcb);

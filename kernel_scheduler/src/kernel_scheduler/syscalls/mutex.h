#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/scheduler/queues.h"
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
  MR_MUTEX_CREATED,
  MR_MUTEX_NAME_ALREADY_EXISTS,
  MR_MUTEX_NAME_NOT_FOUND,
  MR_MUTEX_LOCKED,
  MR_WAITING_MUTEX,
  MR_MUTEX_UNLOCKED,
  MR_PROCESS_HAS_NO_LOCKED_MUTEX
} t_mutex_result;

/** @brief Creates an empty mutex dictionary. */
t_mutex_list* init_list_mutex(void);

/** @brief Destroys every mutex in the list and the list itself. */
void destroy_list_mutex(t_mutex_list* mutex_list);

/**
 * @brief Creates a mutex named @p id if it doesn't already exist.
 * @return MR_MUTEX_CREATED or MR_MUTEX_NAME_ALREADY_EXISTS.
 */
int create_and_add_mutex(t_mutex_list* mutex_list, char* id,
                         bool priority_active, t_queues* queues);

/**
 * @brief Locks the mutex @p id for @p pcb, blocking it (EXEC -> BLOCK) if
 *        already taken.
 * @return MR_MUTEX_NAME_NOT_FOUND, MR_MUTEX_LOCKED or MR_WAITING_MUTEX.
 */
int list_mutex_lock(t_mutex_list* mutex_list, char* id, t_pcb* pcb);

/**
 * @brief Unlocks the mutex @p id, handing it to the next waiter if any.
 * @return MR_MUTEX_NAME_NOT_FOUND, MR_PROCESS_HAS_NO_LOCKED_MUTEX or
 *         MR_MUTEX_UNLOCKED.
 */
int list_mutex_unlock(t_mutex_list* mutex_list, char* id, t_pcb* pcb);

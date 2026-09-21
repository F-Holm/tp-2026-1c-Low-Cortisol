#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils/collections/list.h"
#include "utils/mutex.h"

typedef enum
{
  PS_NEW,
  PS_READY,
  PS_EXEC,
  PS_BLOCK,
  PS_SUSP_BLOCK,
  PS_SUSP_READY,
  PS_EXIT
} t_process_state;

extern const char* const STATE_NAMES[7];

typedef struct
{
  uint32_t pid;
  int priority;
  t_list* priority_list;
  mtx_t priority_mutex;
  unsigned long blocked_time;
  int state;
  mtx_t state_mutex;
  int active_instances;
  mtx_t active_instances_mutex;
  cnd_t no_active_instances;
  void* blocking_mutex;
} t_pcb;

/** @brief Creates a PCB with a fresh PID and a single priority entry. */
t_pcb* create_pcb(int state, int priority);

/** @brief Destroys the PCB's mutexes/conditions/priority list and frees it. */
void destroy_pcb(t_pcb* pcb);

/** @brief Current process state (t_process_state). */
int get_state_pcb(t_pcb* pcb);

/** @brief Current effective priority (the head of priority_list). */
int get_priority_pcb(t_pcb* pcb);

/**
 * @brief Inserts @p pcb into @p list, sorted by priority (best first).
 * @return The index the element was inserted at.
 */
int insert_pcb_sorted(t_list* list, t_pcb* pcb);

/** @brief Increments the count of syscall/worker threads currently touching
 *         this PCB. */
void increment_active_instances(t_pcb* pcb);

/** @brief Decrements the count, signaling any waiter once it reaches zero. */
void decrement_active_instances(t_pcb* pcb);

/** @brief Blocks until active_instances reaches zero. */
void wait_zero_active_instances(t_pcb* pcb);

/** @brief Records the mutex @p pcb is blocked waiting on (NULL to clear). */
void set_mutex_blocking(t_pcb* pcb, void* mutex);

/** @brief Returns the mutex @p pcb is blocked waiting on, or NULL. */
void* get_mutex_blocking(t_pcb* pcb);

#pragma once

#include <stdatomic.h>

#include "utils/sockets.h"

// Counts the processes currently alive in the system. When the count drops
// back to zero the scheduler shuts itself down (SR_NO_PROCESSES).
typedef struct
{
  atomic_int active_process_count;
  t_socket* km_socket;
} t_process_counter;

/** @brief Creates a counter at zero. */
t_process_counter* init_counter_processes(t_socket* km_socket);

/** @brief Increments the count of alive processes. */
void increment_process_count(t_process_counter* counter);

/**
 * @brief Decrements the count; if it reaches zero, shuts the scheduler down
 *        (SR_NO_PROCESSES).
 */
void decrement_process_count(t_process_counter* counter);

/** @brief Frees the counter. */
void destroy_counter_processes(t_process_counter* counter);

#pragma once

#include "kernel_scheduler/scheduler/queue_types.h"

/** @brief Creates a counter at zero. */
t_counter* create_counter(void);

/** @brief Destroys the counter's mutex/condition and frees it. */
void destroy_counter(t_counter* counter);

/** @brief Increments the count. */
void counter_increment(t_counter* counter);

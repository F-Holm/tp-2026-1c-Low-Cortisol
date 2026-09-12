#pragma once

#include "kernel_scheduler/scheduler/queue_types.h"

t_counter* create_counter(void);
void destroy_counter(t_counter* counter);
void counter_increment(t_counter* counter);

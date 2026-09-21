#include "kernel_scheduler/scheduler/counter.h"

#include <stdlib.h>

#include "utils/mutex.h"

t_counter* create_counter(void)
{
  t_counter* counter = malloc(sizeof(t_counter));
  counter->count = 0;
  mtx_init(&(counter->counter_mutex));
  cnd_init(&(counter->condition));
  return counter;
}

void destroy_counter(t_counter* counter)
{
  mtx_destroy(&(counter->counter_mutex));
  cnd_destroy(&(counter->condition));
  free(counter);
}

void counter_increment(t_counter* counter)
{
  mtx_lock(&(counter->counter_mutex));
  counter->count++;
  mtx_unlock(&(counter->counter_mutex));
}

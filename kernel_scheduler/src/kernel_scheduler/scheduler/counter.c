#include "kernel_scheduler/scheduler/counter.h"

#include <stdlib.h>

t_counter* create_counter(void)
{
  t_counter* counter = malloc(sizeof(t_counter));
  counter->count = 0;
  pthread_mutex_init(&(counter->counter_mutex), NULL);
  pthread_cond_init(&(counter->condition), NULL);
  return counter;
}

void destroy_counter(t_counter* counter)
{
  pthread_mutex_destroy(&(counter->counter_mutex));
  pthread_cond_destroy(&(counter->condition));
  free(counter);
}

void counter_increment(t_counter* counter)
{
  pthread_mutex_lock(&(counter->counter_mutex));
  counter->count++;
  pthread_mutex_unlock(&(counter->counter_mutex));
}

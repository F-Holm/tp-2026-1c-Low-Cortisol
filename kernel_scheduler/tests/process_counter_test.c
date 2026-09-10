#include "kernel_scheduler/scheduler/process_counter.h"

#include <criterion/criterion.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"

Test(ks_process_counter, starts_at_zero)
{
  t_kernel_memory_socket* km = init_socket_kernel_memory(-1);
  t_process_counter* c = init_counter_processes(-1, NULL, km);
  cr_assert_eq(atomic_load(&(c->active_process_count)), 0);
  destroy_counter_processes(c);
  destroy_kernel_memory(km);
}

Test(ks_process_counter, increment_and_decrement_that_do_not_reach_zero)
{
  t_kernel_memory_socket* km = init_socket_kernel_memory(-1);
  t_process_counter* c = init_counter_processes(-1, NULL, km);

  increment_process_count(c);
  increment_process_count(c);
  cr_assert_eq(atomic_load(&(c->active_process_count)), 2);

  decrement_process_count(c); /* 2 -> 1, no shutdown */
  cr_assert_eq(atomic_load(&(c->active_process_count)), 1);

  destroy_counter_processes(c);
  destroy_kernel_memory(km);
}

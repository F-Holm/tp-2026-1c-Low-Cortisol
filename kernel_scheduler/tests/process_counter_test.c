#include "kernel_scheduler/scheduler/process_counter.h"

#include <criterion/criterion.h>
#include <stdatomic.h>

#include "support.h"
#include "utils/sockets.h"

Test(ks_process_counter, starts_at_zero)
{
  t_socket* km = ks_dead_socket_with_mutex();
  t_process_counter* c = init_counter_processes(km);
  cr_assert_eq(atomic_load(&(c->active_process_count)), 0);
  destroy_counter_processes(c);
  socket_destroy(km);
}

Test(ks_process_counter, increment_and_decrement_that_do_not_reach_zero)
{
  t_socket* km = ks_dead_socket_with_mutex();
  t_process_counter* c = init_counter_processes(km);

  increment_process_count(c);
  increment_process_count(c);
  cr_assert_eq(atomic_load(&(c->active_process_count)), 2);

  decrement_process_count(c); /* 2 -> 1, no shutdown */
  cr_assert_eq(atomic_load(&(c->active_process_count)), 1);

  destroy_counter_processes(c);
  socket_destroy(km);
}

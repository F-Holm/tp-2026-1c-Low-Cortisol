#include "kernel_scheduler/scheduler/counter.h"

#include <criterion/criterion.h>

Test(ks_counter, starts_at_zero)
{
  t_counter* c = create_counter();
  cr_assert_eq(c->count, 0);
  destroy_counter(c);
}

Test(ks_counter, increment_bumps_the_count)
{
  t_counter* c = create_counter();
  counter_increment(c);
  counter_increment(c);
  counter_increment(c);
  cr_assert_eq(c->count, 3);
  destroy_counter(c);
}

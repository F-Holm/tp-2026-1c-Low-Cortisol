#include "kernel_scheduler/common/time.h"

#include <criterion/criterion.h>

Test(ks_time, time_diff_is_the_absolute_difference)
{
  cr_assert_eq(time_diff(100, 30), 70);
  cr_assert_eq(time_diff(30, 100), 70);
  cr_assert_eq(time_diff(50, 50), 0);
}

Test(ks_time, millis_moves_forward)
{
  unsigned long before = millis();
  unsigned long after = millis();
  cr_assert_geq(after, before);
}

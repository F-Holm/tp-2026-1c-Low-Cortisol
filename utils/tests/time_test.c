#include "utils/time.h"

#include <criterion/criterion.h>

TestSuite(time, .timeout = 5.0);

Test(time, time_sleep_ms_blocks_for_at_least_the_requested_time)
{
  unsigned long before = time_now_ms();
  time_sleep_ms(50);
  unsigned long after = time_now_ms();

  cr_assert_geq(after - before, 50UL);
}

Test(time, time_sleep_ms_accepts_more_than_a_second)
{
  unsigned long before = time_now_ms();
  time_sleep_ms(1100);
  unsigned long after = time_now_ms();

  cr_assert_geq(after - before, 1100UL);
}

Test(time, time_sleep_ms_with_zero_returns_immediately)
{
  time_sleep_ms(0);
}

Test(time, time_now_ms_is_a_plausible_epoch_time_and_never_goes_back)
{
  unsigned long first = time_now_ms();
  unsigned long second = time_now_ms();

  cr_assert_gt(first, 1700000000000UL);
  cr_assert_geq(second, first);
}

Test(time, time_local_now_fields_are_in_range)
{
  t_local_time now = time_local_now();

  cr_assert(now.hour >= 0 && now.hour <= 23);
  cr_assert(now.minute >= 0 && now.minute <= 59);
  cr_assert(now.second >= 0 && now.second <= 60);
  cr_assert(now.millisecond >= 0 && now.millisecond <= 999);
}

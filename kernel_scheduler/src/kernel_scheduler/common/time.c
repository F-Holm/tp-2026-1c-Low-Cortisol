#include "kernel_scheduler/common/time.h"

#include "utils/time.h"

unsigned long millis(void)
{
  return time_now_ms();
}

unsigned long time_diff(unsigned long time_1, unsigned long time_2)
{
  return time_1 > time_2 ? time_1 - time_2 : time_2 - time_1;
}

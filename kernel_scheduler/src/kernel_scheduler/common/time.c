#include "kernel_scheduler/common/time.h"

#include <stddef.h>
#include <sys/time.h>

unsigned long millis(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

unsigned long time_diff(unsigned long time_1, unsigned long time_2)
{
  return time_1 > time_2 ? time_1 - time_2 : time_2 - time_1;
}

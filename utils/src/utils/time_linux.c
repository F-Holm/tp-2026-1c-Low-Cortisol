#include "utils/os.h"

#ifdef OS_LINUX

#include <errno.h>
#include <time.h>

#include "utils/time.h"

void time_sleep_ms(unsigned long milliseconds)
{
  struct timespec remaining = {.tv_sec = milliseconds / 1000,
                               .tv_nsec = (milliseconds % 1000) * 1000000L};

  // nanosleep() returns early when a signal interrupts it; resume with the
  // time it reports as still remaining.
  while (nanosleep(&remaining, &remaining) == -1 && errno == EINTR)
    ;
}

unsigned long time_now_ms(void)
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  return now.tv_sec * 1000UL + now.tv_nsec / 1000000L;
}

t_local_time time_local_now(void)
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  struct tm local_time;
  localtime_r(&now.tv_sec, &local_time);

  return (t_local_time){.hour = local_time.tm_hour,
                        .minute = local_time.tm_min,
                        .second = local_time.tm_sec,
                        .millisecond = now.tv_nsec / 1000000L};
}

#endif  // OS_LINUX

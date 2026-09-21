#pragma once

/**
 * @file
 * @brief Platform-agnostic time helpers: sleeping and wall-clock time.
 *
 * Backed today by time_linux.c. A future Windows backend would provide the
 * same functions in time_windows.c (guarded by #ifdef OS_WINDOWS), so nothing
 * that includes this header needs to change.
 */

/** @brief A wall-clock time of day, in the local time zone. */
typedef struct
{
  int hour;         // 0-23
  int minute;       // 0-59
  int second;       // 0-60 (leap second)
  int millisecond;  // 0-999
} t_local_time;

/** @brief Blocks the calling thread for at least @p milliseconds. */
void time_sleep_ms(unsigned long milliseconds);

/** @brief Milliseconds since the Unix epoch. */
unsigned long time_now_ms(void);

/** @brief The current time of day in the local time zone. */
t_local_time time_local_now(void);

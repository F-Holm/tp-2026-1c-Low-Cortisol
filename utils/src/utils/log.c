#define _GNU_SOURCE

#include "utils/log.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "utils/string.h"

static const char* const LEVEL_NAMES[] = {"TRACE", "DEBUG", "INFO", "WARNING",
                                          "ERROR"};
static const char* const LEVEL_COLORS[] = {"\x1b[36m", "\x1b[32m", "",
                                           "\x1b[33m", "\x1b[31m"};
static const char* const COLOR_RESET = "\x1b[0m";

static long current_thread_id(void)
{
  return syscall(SYS_gettid);
}

static void format_timestamp(char* buffer, size_t size)
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);

  struct tm local_time;
  localtime_r(&now.tv_sec, &local_time);

  char time_part[16];
  strftime(time_part, sizeof(time_part), "%H:%M:%S", &local_time);
  snprintf(buffer, size, "%s:%03ld", time_part, now.tv_nsec / 1000000);
}

static char* format_message(const char* template, va_list arguments)
{
  va_list copy;
  va_copy(copy, arguments);
  int length = vsnprintf(NULL, 0, template, copy);
  va_end(copy);

  char* message = malloc(length + 1);
  vsnprintf(message, length + 1, template, arguments);
  return message;
}

static bool is_level_enabled(t_log* logger, t_log_level level)
{
  return level >= logger->detail;
}

static void log_write(t_log* logger, t_log_level level, const char* template,
                      va_list arguments)
{
  if (!is_level_enabled(logger, level))
  {
    return;
  }

  char timestamp[32];
  format_timestamp(timestamp, sizeof(timestamp));
  char* message = format_message(template, arguments);

  if (logger->file != NULL)
  {
    fprintf(logger->file, "[%s] %s %s/(%d:%ld): %s\n",
            log_level_as_string(level), timestamp, logger->program_name,
            logger->pid, current_thread_id(), message);
    fflush(logger->file);
  }

  if (logger->is_active_console)
  {
    printf("%s[%s] %s %s/(%d:%ld): %s%s\n", LEVEL_COLORS[level],
           log_level_as_string(level), timestamp, logger->program_name,
           logger->pid, current_thread_id(), message, COLOR_RESET);
  }

  free(message);
}

t_log* log_create(char* file, char* program_name, bool is_active_console,
                  t_log_level detail)
{
  FILE* opened = NULL;
  if (file != NULL)
  {
    opened = fopen(file, "a");
    if (opened == NULL)
    {
      return NULL;
    }
  }

  t_log* logger = malloc(sizeof(t_log));
  logger->file = opened;
  logger->is_active_console = is_active_console;
  logger->detail = detail;
  logger->program_name = string_duplicate(program_name);
  logger->pid = getpid();
  return logger;
}

void log_destroy(t_log* logger)
{
  if (logger->file != NULL)
  {
    fclose(logger->file);
  }
  free(logger->program_name);
  free(logger);
}

#define DEFINE_LOG_LEVEL(suffix, level)                      \
  void log_##suffix(t_log* logger, const char* message, ...) \
  {                                                          \
    va_list arguments;                                       \
    va_start(arguments, message);                            \
    log_write(logger, level, message, arguments);            \
    va_end(arguments);                                       \
  }

DEFINE_LOG_LEVEL(trace, LOG_LEVEL_TRACE)
DEFINE_LOG_LEVEL(debug, LOG_LEVEL_DEBUG)
DEFINE_LOG_LEVEL(info, LOG_LEVEL_INFO)
DEFINE_LOG_LEVEL(warning, LOG_LEVEL_WARNING)
DEFINE_LOG_LEVEL(error, LOG_LEVEL_ERROR)

#undef DEFINE_LOG_LEVEL

const char* log_level_as_string(t_log_level level)
{
  return LEVEL_NAMES[level];
}

t_log_level log_level_from_string(char* level)
{
  int amount = sizeof(LEVEL_NAMES) / sizeof(LEVEL_NAMES[0]);
  for (int i = 0; i < amount; i++)
  {
    if (string_equals_ignore_case(level, LEVEL_NAMES[i]))
    {
      return i;
    }
  }
  return -1;
}

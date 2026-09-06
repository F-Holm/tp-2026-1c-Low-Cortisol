#pragma once

#include <pthread.h>
#include <stdbool.h>

#include "utils/log.h"

typedef struct
{
  t_log* log;
  pthread_mutex_t mutex_log;
} t_logger;

// Same functions as utils/log.h, but they take a t_logger and are guarded by an
// internal mutex.

t_logger* logger_create(char* file, char* process_name, bool is_active_console,
                        t_log_level level);

void logger_destroy(t_logger* logger);

#define logger_trace(logger_ptr, ...)                 \
  do                                                  \
  {                                                   \
    pthread_mutex_lock(&((logger_ptr)->mutex_log));   \
    log_trace((logger_ptr)->log, __VA_ARGS__);        \
    pthread_mutex_unlock(&((logger_ptr)->mutex_log)); \
  } while (0)

#define logger_debug(logger_ptr, ...)                 \
  do                                                  \
  {                                                   \
    pthread_mutex_lock(&((logger_ptr)->mutex_log));   \
    log_debug((logger_ptr)->log, __VA_ARGS__);        \
    pthread_mutex_unlock(&((logger_ptr)->mutex_log)); \
  } while (0)

#define logger_info(logger_ptr, ...)                  \
  do                                                  \
  {                                                   \
    pthread_mutex_lock(&((logger_ptr)->mutex_log));   \
    log_info((logger_ptr)->log, __VA_ARGS__);         \
    pthread_mutex_unlock(&((logger_ptr)->mutex_log)); \
  } while (0)

#define logger_warning(logger_ptr, ...)               \
  do                                                  \
  {                                                   \
    pthread_mutex_lock(&((logger_ptr)->mutex_log));   \
    log_warning((logger_ptr)->log, __VA_ARGS__);      \
    pthread_mutex_unlock(&((logger_ptr)->mutex_log)); \
  } while (0)

#define logger_error(logger_ptr, ...)                 \
  do                                                  \
  {                                                   \
    pthread_mutex_lock(&((logger_ptr)->mutex_log));   \
    log_error((logger_ptr)->log, __VA_ARGS__);        \
    pthread_mutex_unlock(&((logger_ptr)->mutex_log)); \
  } while (0)

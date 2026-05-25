#ifndef UTILS_LOGGER_H_
#define UTILS_LOGGER_H_

#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>

typedef struct
{
  t_log* log;
  pthread_mutex_t mutex_log;
} t_logger;

// Todas las funciones son iguales a las de commons/log.h solo que cambian log
// por logger y tienen mutex incorporado

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

#endif /* UTILS_LOGGER_H_ */

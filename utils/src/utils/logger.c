#include "utils/logger.h"

#include <stdlib.h>

t_logger* logger_create(char* file, char* process_name, bool is_active_console,
                        t_log_level level)
{
  t_logger* new_logger = malloc(sizeof(t_logger));
  if (new_logger == NULL)
  {
    return NULL;
  }

  new_logger->log = log_create(file, process_name, is_active_console, level);
  if (new_logger->log == NULL)
  {
    free(new_logger);
    return NULL;
  }

  if (pthread_mutex_init(&(new_logger->mutex_log), NULL) != 0)
  {
    log_destroy(new_logger->log);
    free(new_logger);
    return NULL;
  }

  return new_logger;
}

void logger_destroy(t_logger* logger)
{
  if (logger != NULL)
  {
    log_destroy(logger->log);
    pthread_mutex_destroy(&(logger->mutex_log));
    free(logger);
  }
}

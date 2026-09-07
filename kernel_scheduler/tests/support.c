#include "support.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "utils/log.h"

void ks_init_globals(void)
{
  init_mutex_pid_pcb();
  init_mutex_shutdown();
}

t_log* ks_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "KS-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

t_queues* ks_stub_queues(t_log* logger)
{
  t_queues* queues = calloc(1, sizeof(t_queues));
  queues->logger = logger;
  return queues;
}

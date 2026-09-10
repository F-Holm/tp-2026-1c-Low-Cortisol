#include "support.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "utils/log.h"

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

t_queues* ks_stub_queues_blocking(t_log* logger)
{
  t_queues* queues = calloc(1, sizeof(t_queues));
  queues->logger = logger;
  queues->exec.list = list_create();
  queues->block.list = list_create();
  queues->syscall_counter = calloc(1, sizeof(t_counter));
  pthread_mutex_init(&(queues->syscall_counter->counter_mutex), NULL);
  pthread_cond_init(&(queues->syscall_counter->condition), NULL);
  // Single non-multilevel ready subqueue -- enough for a BLOCK->READY
  // transition (e.g. a process that unblocks once a mutex it was waiting on
  // is released) to have somewhere to land.
  queues->ready.queues = calloc(1, sizeof(t_ready_subqueue));
  queues->ready.queues->queue = list_create();
  return queues;
}

void ks_destroy_stub_queues_blocking(t_queues* queues)
{
  list_destroy(queues->exec.list);
  list_destroy(queues->block.list);
  list_destroy(queues->ready.queues->queue);
  free(queues->ready.queues);
  pthread_mutex_destroy(&(queues->syscall_counter->counter_mutex));
  pthread_cond_destroy(&(queues->syscall_counter->condition));
  free(queues->syscall_counter);
  free(queues);
}

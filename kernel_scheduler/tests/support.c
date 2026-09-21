#include "support.h"

#include <criterion/criterion.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/counter.h"
#include "kernel_scheduler/scheduler/exec_list.h"
#include "kernel_scheduler/scheduler/process_counter.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"

t_log* ks_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "KS-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

// A socket that already failed: it has its mutex but every send/receive on it
// fails, which is what the stub queues need to exercise the error paths.
t_socket* ks_dead_socket_with_mutex(void)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener);
  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));

  t_socket* dead = socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, true);
  cr_assert_not_null(dead);
  socket_destroy(listener);
  socket_close(dead);
  return dead;
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
  queues->routines.syscall_counter = calloc(1, sizeof(t_counter));
  mtx_init(&(queues->routines.syscall_counter->counter_mutex));
  cnd_init(&(queues->routines.syscall_counter->condition));
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
  mtx_destroy(&(queues->routines.syscall_counter->counter_mutex));
  cnd_destroy(&(queues->routines.syscall_counter->condition));
  free(queues->routines.syscall_counter);
  free(queues);
}

t_queues* ks_stub_queues_full(t_log* logger)
{
  t_queues* queues = calloc(1, sizeof(t_queues));
  queues->logger = logger;
  init_ready_queue(&(queues->ready), SA_FIFO, NULL);
  init_exec_list(&(queues->exec), 0, false);
  init_blocking_list(&(queues->block));
  init_blocking_list(&(queues->susp_block));
  init_blocking_list(&(queues->susp_ready));
  queues->routines.thread_counter = create_counter();
  queues->routines.syscall_counter = create_counter();
  queues->km_socket = ks_dead_socket_with_mutex();
  queues->process_counter = init_counter_processes(queues->km_socket);
  init_shutdown(NULL, logger, NULL);
  atomic_init(&(queues->routines.compaction_active), false);
  atomic_init(&(queues->routines.resume_active), false);
  mtx_init(&(queues->routines.routine_mutex));
  cnd_init(&(queues->routines.routine_cond));
  return queues;
}

void ks_destroy_stub_queues_full(t_queues* queues)
{
  destroy_ready_queue(&(queues->ready));
  destroy_exec_list(&(queues->exec));
  destroy_blocking_list(&(queues->block));
  destroy_blocking_list(&(queues->susp_block));
  destroy_blocking_list(&(queues->susp_ready));
  destroy_counter(queues->routines.thread_counter);
  destroy_counter(queues->routines.syscall_counter);
  destroy_counter_processes(queues->process_counter);
  socket_destroy(queues->km_socket);
  mtx_destroy(&(queues->routines.routine_mutex));
  cnd_destroy(&(queues->routines.routine_cond));
  free(queues);
}

t_socket* ks_listen_ephemeral(char* port_out, int port_len)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  snprintf(port_out, port_len, "%d", socket_get_local_port(listener));

  return listener;
}

static t_socket* connected_pair(t_socket** server_out, bool with_mutex)
{
  char port[16];
  t_socket* listener = ks_listen_ephemeral(port, sizeof(port));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, with_mutex);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, with_mutex);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

t_socket* ks_connected_pair(t_socket** server_out)
{
  return connected_pair(server_out, false);
}

t_socket* ks_connected_pair_with_mutex(t_socket** server_out)
{
  return connected_pair(server_out, true);
}

void ks_wait_thread_counter_zero(t_queues* queues)
{
  mtx_lock(&(queues->routines.thread_counter->counter_mutex));
  while (queues->routines.thread_counter->count > 0)
  {
    cnd_wait(&(queues->routines.thread_counter->condition),
             &(queues->routines.thread_counter->counter_mutex));
  }
  mtx_unlock(&(queues->routines.thread_counter->counter_mutex));
}

t_ks_file_logger ks_open_file_logger(void)
{
  t_ks_file_logger file_logger = {.path = "/tmp/ks_file_logger_XXXXXX"};
  int fd = mkstemp(file_logger.path);
  cr_assert_neq(fd, -1);
  close(fd);
  file_logger.logger =
      log_create(file_logger.path, "KS-test", false, LOG_LEVEL_TRACE, true);
  cr_assert_not_null(file_logger.logger);
  return file_logger;
}

bool ks_file_logger_contains(t_ks_file_logger* file_logger, const char* text)
{
  FILE* file = fopen(file_logger->path, "r");
  cr_assert_not_null(file);
  char line[512];
  bool found = false;
  while (fgets(line, sizeof(line), file) != NULL)
  {
    if (strstr(line, text) != NULL)
      found = true;
  }
  fclose(file);
  return found;
}

void ks_close_file_logger(t_ks_file_logger* file_logger)
{
  log_destroy(file_logger->logger);
  unlink(file_logger->path);
}

#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/msg.h"

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

t_queues* ks_stub_queues_full(t_log* logger)
{
  t_queues* queues = calloc(1, sizeof(t_queues));
  queues->logger = logger;
  queues->server_socket = -1;
  init_ready_queue(&(queues->ready), AP_FIFO, NULL);
  init_exec_list(&(queues->exec), 0, false);
  init_blocking_list(&(queues->block));
  init_blocking_list(&(queues->susp_block));
  init_blocking_list(&(queues->susp_ready));
  queues->thread_counter = create_counter();
  queues->syscall_counter = create_counter();
  queues->km_socket = init_socket_kernel_memory(-1);
  queues->process_counter =
      init_counter_processes(-1, logger, queues->km_socket);
  atomic_init(&(queues->compaction_active), false);
  atomic_init(&(queues->resume_active), false);
  pthread_mutex_init(&(queues->routine_mutex), NULL);
  pthread_cond_init(&(queues->routine_cond), NULL);
  return queues;
}

void ks_destroy_stub_queues_full(t_queues* queues)
{
  destroy_ready_queue(&(queues->ready));
  destroy_exec_list(&(queues->exec));
  destroy_blocking_list(&(queues->block));
  destroy_blocking_list(&(queues->susp_block));
  destroy_blocking_list(&(queues->susp_ready));
  destroy_counter(queues->thread_counter);
  destroy_counter(queues->syscall_counter);
  destroy_counter_processes(queues->process_counter);
  destroy_kernel_memory(queues->km_socket);
  pthread_mutex_destroy(&(queues->routine_mutex));
  pthread_cond_destroy(&(queues->routine_cond));
  free(queues);
}

int ks_listen_ephemeral(char* port_out, int port_len)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  snprintf(port_out, port_len, "%d", ntohs(address.sin_port));

  return listen_fd;
}

int ks_connected_pair(int* server_out)
{
  char port[16];
  int listen_fd = ks_listen_ephemeral(port, sizeof(port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

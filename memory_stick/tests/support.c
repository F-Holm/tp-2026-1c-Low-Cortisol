#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/msg.h"

t_log* ms_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "MS-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

int ms_connected_pair(int* server_out)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  char port[16];
  snprintf(port, sizeof(port), "%d", ntohs(address.sin_port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

t_ms* ms_make(int memory_size)
{
  t_ms* ms = calloc(1, sizeof(t_ms));
  ms->memory = calloc(memory_size, 1);
  ms->memory_delay = 0;
  ms->logger = ms_quiet_logger();
  ms->memory_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(ms->memory_mutex, NULL);
  return ms;
}

void ms_destroy(t_ms* ms)
{
  pthread_mutex_destroy(ms->memory_mutex);
  free(ms->memory_mutex);
  free(ms->memory);
  log_destroy(ms->logger);
  free(ms);
}

char* ms_write_temp_config(void)
{
  char path[] = "/tmp/ms_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1, "could not create a temp config file");

  FILE* file = fdopen(fd, "w");
  fputs(
      "LOG_LEVEL=INFO\n"
      "MEMORY_DELAY=0\n"
      "KERNEL_MEMORY_IP=127.0.0.1\n"
      "KERNEL_MEMORY_PORT=4321\n",
      file);
  fclose(file);

  return strdup(path);
}

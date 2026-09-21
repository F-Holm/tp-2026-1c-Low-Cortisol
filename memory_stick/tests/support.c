#include "support.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory_stick/memory_stick.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"

t_log* ms_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "MS-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

t_socket* ms_connected_pair(t_socket** server_out)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

t_ms* ms_make(int memory_size)
{
  t_ms* ms = calloc(1, sizeof(t_ms));
  ms->memory = calloc(memory_size, 1);
  ms->memory_delay = 0;
  ms->logger = ms_quiet_logger();
  ms->memory_mutex = malloc(sizeof(mtx_t));
  mtx_init(ms->memory_mutex);
  return ms;
}

void ms_destroy(t_ms* ms)
{
  mtx_destroy(ms->memory_mutex);
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

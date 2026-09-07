#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/msg.h"

int io_listen_ephemeral(char* port_out, int port_len)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  snprintf(port_out, port_len, "%d", ntohs(address.sin_port));

  return listen_fd;
}

int io_connected_pair(int* server_out)
{
  char port[16];
  int listen_fd = io_listen_ephemeral(port, sizeof(port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

char* io_write_temp_config(void)
{
  char path[] = "/tmp/io_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1, "could not create a temp config file");

  FILE* file = fdopen(fd, "w");
  fputs(
      "LOG_LEVEL=INFO\n"
      "KERNEL_SCHEDULER_IP=127.0.0.1\n"
      "KERNEL_SCHEDULER_PORT=1234\n",
      file);
  fclose(file);

  return strdup(path);
}

t_log* io_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "IO-test", false, LOG_LEVEL_ERROR, false);
  cr_assert_not_null(logger);
  return logger;
}

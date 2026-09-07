#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/msg.h"

int swap_listen_ephemeral(char* port_out, int port_len)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  snprintf(port_out, port_len, "%d", ntohs(address.sin_port));

  return listen_fd;
}

int swap_connected_pair(int* server_out)
{
  char port[16];
  int listen_fd = swap_listen_ephemeral(port, sizeof(port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

char* swap_write_temp_config(const char* swap_file_path)
{
  char path[] = "/tmp/swap_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1, "could not create a temp config file");

  FILE* file = fdopen(fd, "w");
  fprintf(file,
          "LOG_LEVEL=INFO\n"
          "KERNEL_MEMORY_IP=127.0.0.1\n"
          "KERNEL_MEMORY_PORT=1234\n"
          "SWAP_FILE_PATH=%s\n"
          "SWAP_FILE_SIZE=4096\n"
          "BLOCK_SIZE=64\n",
          swap_file_path);
  fclose(file);

  return strdup(path);
}

t_log* swap_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "SWAP-test", false, LOG_LEVEL_ERROR, false);
  cr_assert_not_null(logger);
  return logger;
}

FILE* swap_sized_tmpfile(int size)
{
  FILE* file = tmpfile();
  cr_assert_not_null(file);
  cr_assert_eq(ftruncate(fileno(file), size), 0);
  return file;
}

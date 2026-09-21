#include "support.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/file.h"
#include "utils/msg.h"

t_socket* swap_listen_ephemeral(char* port_out, int port_len)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  snprintf(port_out, port_len, "%d", socket_get_local_port(listener));

  return listener;
}

t_socket* swap_connected_pair(t_socket** server_out)
{
  char port[16];
  t_socket* listener = swap_listen_ephemeral(port, sizeof(port));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
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
  cr_assert(file_resize(file, size));
  return file;
}

#include "support.h"

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/msg.h"

t_log* km_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "KM-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

t_hole* km_make_hole(int base, int size)
{
  t_hole* hole = malloc(sizeof(t_hole));
  hole->base = base;
  hole->size = size;
  return hole;
}

t_segment* km_make_segment(uint32_t id, uint32_t pid, int base, int size)
{
  t_segment* segment = calloc(1, sizeof(t_segment));
  segment->id = id;
  segment->pid = pid;
  segment->base = base;
  segment->size = size;
  return segment;
}

t_stick_data* km_make_stick(int size)
{
  t_stick_data* stick = calloc(1, sizeof(t_stick_data));
  stick->stick_size = size;
  return stick;
}

t_socket* km_listen_ephemeral(char* port_out, int port_len)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  snprintf(port_out, port_len, "%d", socket_get_local_port(listener));

  return listener;
}

t_socket* km_connected_pair(t_socket** server_out)
{
  char port[16];
  t_socket* listener = km_listen_ephemeral(port, sizeof(port));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

#include "memory_stick/kernel_memory.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "support.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

/* Sockets everywhere -- keep a hard ceiling so a protocol mistake fails fast.
 */
TestSuite(ms_km, .timeout = 10.0);

static t_socket* listen_ephemeral(char* port, int len)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener);
  snprintf(port, len, "%d", socket_get_local_port(listener));
  return listener;
}

/* ── send_size / send_cpu_server_port_to_km ────────────────────────────── */

Test(ms_km, send_size_sends_the_size_string)
{
  t_socket* km_fd;
  t_socket* stick_fd = ms_connected_pair(&km_fd);
  t_log* logger = ms_quiet_logger();

  cr_assert(send_size(stick_fd, "4096", logger));

  cr_assert_eq(receive_op_code(km_fd), OP_MEMORY_SIZE);
  char* size = receive_string(km_fd);
  cr_assert_str_eq(size, "4096");
  free(size);

  socket_destroy(stick_fd);
  socket_destroy(km_fd);
  log_destroy(logger);
}

Test(ms_km, send_cpu_server_port_to_km_sends_the_port_as_a_string)
{
  t_socket* km_fd;
  t_socket* stick_fd = ms_connected_pair(&km_fd);

  cr_assert(send_cpu_server_port_to_km(stick_fd, 5123));

  cr_assert_eq(receive_op_code(km_fd), OP_PORT);
  char* port = receive_string(km_fd);
  cr_assert_str_eq(port, "5123");
  free(port);

  socket_destroy(stick_fd);
  socket_destroy(km_fd);
}

/* ── connect_km ────────────────────────────────────────────────────────── */

Test(ms_km, connect_km_fails_when_nothing_is_listening)
{
  char port[16];
  t_socket* listen_fd = listen_ephemeral(port, sizeof(port));
  socket_destroy(listen_fd); /* the port is now free */

  t_log* logger = ms_quiet_logger();
  cr_assert_eq(connect_km("127.0.0.1", port, logger), NULL);
  log_destroy(logger);
}

/* ── handshake_km / connect_to_kernel_memory ───────────────────────────── */

struct km_side
{
  t_socket* listen_fd;
  int received_id;
  int announce_as;
  char* size;
};

static int km_thread(void* arg)
{
  struct km_side* side = arg;
  t_socket* client = socket_accept(side->listen_fd, false);
  if (client == NULL)
  {
    return 0;
  }
  side->received_id = receive_handshake(client);
  send_handshake(side->announce_as, client);
  if (receive_op_code(client) == OP_MEMORY_SIZE)
  {
    side->size = receive_string(client);
  }
  socket_destroy(client);
  return 0;
}

Test(ms_km, connect_to_kernel_memory_handshakes_and_sends_the_size)
{
  char port[16];
  t_socket* listen_fd = listen_ephemeral(port, sizeof(port));

  struct km_side side = {
      .listen_fd = listen_fd,
      .received_id = -1,
      .announce_as = MID_KERNEL_MEMORY,
  };
  thrd_t thread;
  thrd_create(&thread, km_thread, &side);

  t_log* logger = ms_quiet_logger();
  t_socket* socket_km =
      connect_to_kernel_memory("127.0.0.1", port, "8192", logger);
  cr_assert_not_null(socket_km);

  thrd_join(thread, NULL);
  cr_assert_eq(side.received_id, MID_MEMORY_STICK);
  cr_assert_str_eq(side.size, "8192");

  free(side.size);
  socket_destroy(socket_km);
  socket_destroy(listen_fd);
  log_destroy(logger);
}

Test(ms_km, connect_to_kernel_memory_fails_when_the_peer_is_wrong)
{
  char port[16];
  t_socket* listen_fd = listen_ephemeral(port, sizeof(port));

  struct km_side side = {
      .listen_fd = listen_fd,
      .received_id = -1,
      .announce_as = MID_CPU, /* wrong */
  };
  thrd_t thread;
  thrd_create(&thread, km_thread, &side);

  t_log* logger = ms_quiet_logger();
  cr_assert_eq(connect_to_kernel_memory("127.0.0.1", port, "8192", logger),
               NULL);

  thrd_join(thread, NULL);
  free(side.size);
  socket_destroy(listen_fd);
  log_destroy(logger);
}

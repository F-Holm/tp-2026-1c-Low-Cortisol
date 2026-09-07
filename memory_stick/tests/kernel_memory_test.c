#include "memory_stick/kernel_memory.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

/* Sockets everywhere -- keep a hard ceiling so a protocol mistake fails fast.
 */
TestSuite(ms_km, .timeout = 10.0);

static int listen_ephemeral(char* port, int len)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0);
  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  snprintf(port, len, "%d", ntohs(address.sin_port));
  return listen_fd;
}

/* ── send_size / send_cpu_server_port_to_km ────────────────────────────── */

Test(ms_km, send_size_sends_the_size_string)
{
  int km_fd;
  int stick_fd = ms_connected_pair(&km_fd);
  t_log* logger = ms_quiet_logger();

  cr_assert(send_size(stick_fd, "4096", logger));

  cr_assert_eq(receive_op_code(km_fd), OP_MEMORY_SIZE);
  char* size = receive_string(km_fd);
  cr_assert_str_eq(size, "4096");
  free(size);

  close(stick_fd);
  close(km_fd);
  log_destroy(logger);
}

Test(ms_km, send_cpu_server_port_to_km_sends_the_port_as_a_string)
{
  int km_fd;
  int stick_fd = ms_connected_pair(&km_fd);

  cr_assert(send_cpu_server_port_to_km(stick_fd, 5123));

  cr_assert_eq(receive_op_code(km_fd), OP_PORT);
  char* port = receive_string(km_fd);
  cr_assert_str_eq(port, "5123");
  free(port);

  close(stick_fd);
  close(km_fd);
}

/* ── connect_km ────────────────────────────────────────────────────────── */

Test(ms_km, connect_km_fails_when_nothing_is_listening)
{
  char port[16];
  int listen_fd = listen_ephemeral(port, sizeof(port));
  close(listen_fd); /* the port is now free */

  t_log* logger = ms_quiet_logger();
  cr_assert_eq(connect_km("127.0.0.1", port, logger), -1);
  log_destroy(logger);
}

/* ── handshake_km / connect_to_kernel_memory ───────────────────────────── */

struct km_side
{
  int listen_fd;
  int received_id;
  int announce_as;
  char* size;
};

static void* km_thread(void* arg)
{
  struct km_side* side = arg;
  int client = accept(side->listen_fd, NULL, NULL);
  if (client < 0)
  {
    return NULL;
  }
  /* So a failed handshake on the client side does not leave this thread blocked
   * in receive_op_code() forever. */
  struct timeval timeout = {.tv_sec = 3};
  setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  side->received_id = receive_handshake(client);
  send_handshake(side->announce_as, client);
  if (receive_op_code(client) == OP_MEMORY_SIZE)
  {
    side->size = receive_string(client);
  }
  close(client);
  return NULL;
}

Test(ms_km, connect_to_kernel_memory_handshakes_and_sends_the_size)
{
  char port[16];
  int listen_fd = listen_ephemeral(port, sizeof(port));

  struct km_side side = {
      .listen_fd = listen_fd,
      .received_id = -1,
      .announce_as = MID_KERNEL_MEMORY,
  };
  pthread_t thread;
  pthread_create(&thread, NULL, km_thread, &side);

  t_log* logger = ms_quiet_logger();
  int socket_km = connect_to_kernel_memory("127.0.0.1", port, "8192", logger);
  cr_assert_gt(socket_km, 0);

  pthread_join(thread, NULL);
  cr_assert_eq(side.received_id, MID_MEMORY_STICK);
  cr_assert_str_eq(side.size, "8192");

  free(side.size);
  close(socket_km);
  close(listen_fd);
  log_destroy(logger);
}

Test(ms_km, connect_to_kernel_memory_fails_when_the_peer_is_wrong)
{
  char port[16];
  int listen_fd = listen_ephemeral(port, sizeof(port));

  struct km_side side = {
      .listen_fd = listen_fd,
      .received_id = -1,
      .announce_as = MID_CPU, /* wrong */
  };
  pthread_t thread;
  pthread_create(&thread, NULL, km_thread, &side);

  t_log* logger = ms_quiet_logger();
  cr_assert_eq(connect_to_kernel_memory("127.0.0.1", port, "8192", logger), -1);

  pthread_join(thread, NULL);
  free(side.size);
  close(listen_fd);
  log_destroy(logger);
}

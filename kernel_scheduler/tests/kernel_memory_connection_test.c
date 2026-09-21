#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/connections/kernel_memory.h"
#include "kernel_scheduler/shutdown.h"
#include "support.h"
#include "utils/msg.h"
#include "utils/threads.h"

/* ── start_connection_kernel_memory ───────────────────────────────────── */

struct km_stub
{
  t_socket* listen_fd;
  int announce_as;
  int received_mid;
};

static void* km_stub_thread(void* arg)
{
  struct km_stub* stub = arg;
  t_socket* client = socket_accept(stub->listen_fd, false);
  if (client < 0)
    return NULL;
  stub->received_mid = receive_handshake(client);
  send_handshake(stub->announce_as, client);
  socket_destroy(client);
  return NULL;
}

Test(ks_km_connection, succeeds_when_the_peer_announces_kernel_memory)
{
  char port[16];
  t_socket* listen_fd = ks_listen_ephemeral(port, sizeof(port));
  struct km_stub stub = {.listen_fd = listen_fd,
                         .announce_as = MID_KERNEL_MEMORY,
                         .received_mid = -1};
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);

  t_log* logger = ks_quiet_logger();
  t_socket* km_socket =
      start_connection_kernel_memory("127.0.0.1", port, logger);

  thrd_join(thread, NULL);
  cr_assert_not_null(km_socket);
  cr_assert_eq(stub.received_mid, MID_KERNEL_SCHEDULER);

  socket_destroy(km_socket);
  socket_destroy(listen_fd);
  log_destroy(logger);
}

Test(ks_km_connection, fails_when_the_peer_is_not_kernel_memory)
{
  char port[16];
  t_socket* listen_fd = ks_listen_ephemeral(port, sizeof(port));
  struct km_stub stub = {
      .listen_fd = listen_fd, .announce_as = MID_CPU, .received_mid = -1};
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);

  t_log* logger = ks_quiet_logger();
  t_socket* km_socket =
      start_connection_kernel_memory("127.0.0.1", port, logger);

  thrd_join(thread, NULL);
  cr_assert_null(km_socket);

  socket_destroy(listen_fd);
  log_destroy(logger);
}

Test(ks_km_connection, fails_when_kernel_memory_is_unreachable)
{
  char port[16];
  t_socket* listen_fd = ks_listen_ephemeral(port, sizeof(port));
  socket_destroy(listen_fd); /* nothing is listening any more */

  t_log* logger = ks_quiet_logger();
  cr_assert_null(start_connection_kernel_memory("127.0.0.1", port, logger));
  log_destroy(logger);
}

/* ── notify_terminate_process ─────────────────────────────────────────── */

Test(ks_km_connection, notify_terminate_process_sends_the_pid)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair_with_mutex(&server_fd);
  t_socket* km_socket = client_fd;
  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  cr_assert(notify_terminate_process(km_socket, 42));

  cr_assert_eq(receive_op_code(server_fd), OP_END_PROCESS);
  int size;
  uint32_t* pid = receive_buffer(&size, server_fd);
  cr_assert_eq(*pid, 42);

  free(pid);
  socket_destroy(server_fd);
  socket_destroy(km_socket);
  log_destroy(logger);
}

Test(ks_km_connection, notify_terminate_process_reports_a_dead_socket)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair_with_mutex(&server_fd);
  socket_shutdown(
      client_fd,
      SOCKET_SHUTDOWN_WRITE); /* the send() now fails deterministically */
  /* On a send failure, close_kernel_scheduler reads km_socket once more to
   * classify the failure -- close the peer too so that read returns at once
   * instead of blocking forever waiting for a message nobody will send. */
  socket_destroy(server_fd);
  t_socket* km_socket = client_fd;
  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  cr_assert_not(notify_terminate_process(km_socket, 42));

  socket_destroy(km_socket);
  log_destroy(logger);
}

/* ── the connection-check watchdog thread ─────────────────────────────── */

Test(ks_km_connection, the_watchdog_thread_stops_when_asked_to)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair_with_mutex(&server_fd);
  t_socket* km_socket = client_fd;
  t_log* logger = ks_quiet_logger();

  t_connection_check_thread* data =
      start_thread_check_connection_kernel_memory(logger, km_socket);
  cr_assert_not_null(data);

  destroy_thread_check_connection_kernel_memory(data); /* joins the thread */

  socket_destroy(server_fd);
  socket_destroy(km_socket);
  log_destroy(logger);
}

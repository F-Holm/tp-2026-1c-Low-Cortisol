#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/connections/kernel_memory.h"
#include "support.h"
#include "utils/msg.h"

/* ── start_connection_kernel_memory ───────────────────────────────────── */

struct km_stub
{
  int listen_fd;
  int announce_as;
  int received_mid;
};

static void* km_stub_thread(void* arg)
{
  struct km_stub* stub = arg;
  int client = accept(stub->listen_fd, NULL, NULL);
  if (client < 0)
    return NULL;
  stub->received_mid = receive_handshake(client);
  send_handshake(stub->announce_as, client);
  close(client);
  return NULL;
}

Test(ks_km_connection, succeeds_when_the_peer_announces_kernel_memory)
{
  char port[16];
  int listen_fd = ks_listen_ephemeral(port, sizeof(port));
  struct km_stub stub = {.listen_fd = listen_fd,
                         .announce_as = MID_KERNEL_MEMORY,
                         .received_mid = -1};
  pthread_t thread;
  pthread_create(&thread, NULL, km_stub_thread, &stub);

  t_log* logger = ks_quiet_logger();
  int km_socket = start_connection_kernel_memory("127.0.0.1", port, logger);

  pthread_join(thread, NULL);
  cr_assert_geq(km_socket, 0);
  cr_assert_eq(stub.received_mid, MID_KERNEL_SCHEDULER);

  close(km_socket);
  close(listen_fd);
  log_destroy(logger);
}

Test(ks_km_connection, fails_when_the_peer_is_not_kernel_memory)
{
  char port[16];
  int listen_fd = ks_listen_ephemeral(port, sizeof(port));
  struct km_stub stub = {
      .listen_fd = listen_fd, .announce_as = MID_CPU, .received_mid = -1};
  pthread_t thread;
  pthread_create(&thread, NULL, km_stub_thread, &stub);

  t_log* logger = ks_quiet_logger();
  int km_socket = start_connection_kernel_memory("127.0.0.1", port, logger);

  pthread_join(thread, NULL);
  cr_assert_eq(km_socket, -1);

  close(listen_fd);
  log_destroy(logger);
}

Test(ks_km_connection, fails_when_kernel_memory_is_unreachable)
{
  char port[16];
  int listen_fd = ks_listen_ephemeral(port, sizeof(port));
  close(listen_fd); /* nothing is listening any more */

  t_log* logger = ks_quiet_logger();
  cr_assert_eq(start_connection_kernel_memory("127.0.0.1", port, logger), -1);
  log_destroy(logger);
}

/* ── notify_terminate_process ─────────────────────────────────────────── */

Test(ks_km_connection, notify_terminate_process_sends_the_pid)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  t_kernel_memory_socket* km_socket = init_socket_kernel_memory(client_fd);
  t_log* logger = ks_quiet_logger();

  cr_assert(notify_terminate_process(km_socket, 42, -1, logger));

  cr_assert_eq(receive_op_code(server_fd), OP_END_PROCESS);
  int size;
  uint32_t* pid = receive_buffer(&size, server_fd);
  cr_assert_eq(*pid, 42);

  free(pid);
  close(server_fd);
  destroy_kernel_memory(km_socket);
  log_destroy(logger);
}

Test(ks_km_connection, notify_terminate_process_reports_a_dead_socket)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  shutdown(client_fd, SHUT_WR); /* the send() now fails deterministically */
  /* On a send failure, close_kernel_scheduler reads km_socket once more to
   * classify the failure -- close the peer too so that read returns at once
   * instead of blocking forever waiting for a message nobody will send. */
  close(server_fd);
  t_kernel_memory_socket* km_socket = init_socket_kernel_memory(client_fd);
  t_log* logger = ks_quiet_logger();

  cr_assert_not(notify_terminate_process(km_socket, 42, -1, logger));

  destroy_kernel_memory(km_socket);
  log_destroy(logger);
}

/* ── the connection-check watchdog thread ─────────────────────────────── */

Test(ks_km_connection, the_watchdog_thread_stops_when_asked_to)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  t_kernel_memory_socket* km_socket = init_socket_kernel_memory(client_fd);
  t_log* logger = ks_quiet_logger();

  t_connection_check_thread* data =
      start_thread_check_connection_kernel_memory(-1, logger, km_socket);
  cr_assert_not_null(data);

  destroy_thread_check_connection_kernel_memory(data); /* joins the thread */

  close(server_fd);
  destroy_kernel_memory(km_socket);
  log_destroy(logger);
}

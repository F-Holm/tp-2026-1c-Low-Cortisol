#include "memory_stick/cpu.h"

#include <criterion/criterion.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

/* Several of these spin up a peer thread -- cap every test so a stalled
 * handshake fails instead of hanging the run. */
TestSuite(ms_cpu_server, .timeout = 10.0);
TestSuite(ms_cpu_thread_data, .timeout = 10.0);
TestSuite(ms_cpu_iterator, .timeout = 10.0);
TestSuite(ms_handshake_cpu, .timeout = 10.0);
TestSuite(ms_receive_cpu_id, .timeout = 10.0);

/* ── create_server_cpu / get_cpu_port ──────────────────────────────────── */

Test(ms_cpu_server, create_server_cpu_opens_a_listening_socket_with_a_port)
{
  t_log* logger = ms_quiet_logger();

  int server = create_server_cpu(logger);
  cr_assert_gt(server, 0);
  cr_assert_gt(get_cpu_port(server), 0);

  close(server);
  log_destroy(logger);
}

/* ── create_cpu_thread_data ────────────────────────────────────────────── */

Test(ms_cpu_thread_data, keeps_every_field)
{
  t_list list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  t_ms ms;

  t_cpu_thread* data = create_cpu_thread_data(42, &list, &mutex, &cond, &ms);
  cr_assert_eq(data->socket_cpu, 42);
  cr_assert_eq(data->socket_list, &list);
  cr_assert_eq(data->socket_list_mutex, &mutex);
  cr_assert_eq(data->listen_done_cond, &cond);
  cr_assert_eq(data->ms, &ms);
  free(data);
}

/* ── iterator_shutdown ─────────────────────────────────────────────────── */

Test(ms_cpu_iterator, iterator_shutdown_stops_the_socket)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

  iterator_shutdown(&fds[0]);

  char byte;
  cr_assert_eq(read(fds[0], &byte, 1), 0); /* shutdown -> EOF */

  close(fds[0]);
  close(fds[1]);
}

/* ── handshake_cpu ─────────────────────────────────────────────────────── */

struct cpu_side
{
  int fd;
  bool ok;
};

static void* cpu_handshake_thread(void* arg)
{
  struct cpu_side* side = arg;
  send_handshake(MID_CPU, side->fd);
  side->ok = receive_handshake(side->fd) == MID_MEMORY_STICK;
  return NULL;
}

Test(ms_handshake_cpu, exchanges_the_handshake_with_a_cpu)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  struct cpu_side side = {.fd = cpu_fd};
  pthread_t thread;
  pthread_create(&thread, NULL, cpu_handshake_thread, &side);

  cr_assert(handshake_cpu(stick_fd, logger));

  pthread_join(thread, NULL);
  cr_assert(side.ok);

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

Test(ms_handshake_cpu, fails_when_the_peer_is_not_a_cpu)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_handshake(MID_KERNEL_SCHEDULER, cpu_fd); /* not MID_CPU */
  cr_assert_not(handshake_cpu(stick_fd, logger));

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

/* ── receive_cpu_id ────────────────────────────────────────────────────── */

Test(ms_receive_cpu_id, reads_the_id_after_the_op_code)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_string(OP_ID_CPU, "CPU-3", cpu_fd);

  char* id = receive_cpu_id(stick_fd, logger);
  cr_assert_not_null(id);
  cr_assert_str_eq(id, "CPU-3");
  free(id);

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

Test(ms_receive_cpu_id, rejects_a_wrong_op_code)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_string(OP_HANDSHAKE, "CPU-3", cpu_fd);
  cr_assert_null(receive_cpu_id(stick_fd, logger));

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

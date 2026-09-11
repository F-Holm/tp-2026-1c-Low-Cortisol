#include <criterion/criterion.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/connections/io.h"
#include "support.h"
#include "utils/io.h"
#include "utils/msg.h"

Test(ks_io_connection, succeeds_and_starts_a_worker_thread_for_a_valid_io_type)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, fds[0], queues, false, -1));
  cr_assert_eq(receive_handshake(fds[1]), MID_KERNEL_SCHEDULER);
  cr_assert_eq(io[E_STDIN].socket_io, fds[0]);

  close_io(io);
  close(fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_when_the_io_type_is_unknown)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "NOT_A_TYPE", fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert_not(handle_new_io(io, fds[0], queues, false, -1));

  close_io(io);
  close(fds[0]);
  close(fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_when_the_io_type_is_a_duplicate)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", fds[1]));
  int dup_fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, dup_fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", dup_fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, fds[0], queues, false, -1));
  cr_assert_not(handle_new_io(io, dup_fds[0], queues, false, -1));

  close_io(io);
  close(fds[1]);
  close(dup_fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_gracefully_on_a_dead_socket)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert_not(handle_new_io(io, -1, queues, false, -1));

  close_io(io);
  free(queues);
  log_destroy(logger);
}

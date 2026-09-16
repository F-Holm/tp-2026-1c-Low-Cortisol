#include "kernel_memory/error.h"

#include <criterion/criterion.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "support.h"

Test(km_error, send_handshake_error_closes_the_socket)
{
  int fds[2];
  cr_assert_eq(pipe(fds), 0);

  t_log* logger = km_quiet_logger();
  send_handshake_error(logger, fds[0], "Kernel Scheduler");

  cr_assert_eq(fcntl(fds[0], F_GETFD), -1, "socket fd should be closed");
  cr_assert_eq(errno, EBADF);

  close(fds[1]);
  log_destroy(logger);
}

Test(km_error, send_init_error_closes_the_socket)
{
  int fds[2];
  cr_assert_eq(pipe(fds), 0);

  t_log* logger = km_quiet_logger();
  send_init_error(logger, fds[0], "Memory Stick");

  cr_assert_eq(fcntl(fds[0], F_GETFD), -1, "socket fd should be closed");
  cr_assert_eq(errno, EBADF);

  close(fds[1]);
  log_destroy(logger);
}

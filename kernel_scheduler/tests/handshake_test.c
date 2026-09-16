#include "kernel_scheduler/common/handshake.h"

#include <criterion/criterion.h>
#include <sys/socket.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

Test(ks_handshake, sends_the_module_id_over_a_live_socket)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  t_log* logger = ks_quiet_logger();

  cr_assert(respond_handshake(fds[0], MID_KERNEL_SCHEDULER, logger));
  cr_assert_eq(receive_handshake(fds[1]), MID_KERNEL_SCHEDULER);

  close(fds[0]);
  close(fds[1]);
  log_destroy(logger);
}

Test(ks_handshake, reports_failure_on_a_dead_socket)
{
  t_log* logger = ks_quiet_logger();
  cr_assert_not(respond_handshake(-1, MID_KERNEL_SCHEDULER, logger));
  log_destroy(logger);
}

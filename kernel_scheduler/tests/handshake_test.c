#include "kernel_scheduler/common/handshake.h"

#include <criterion/criterion.h>

#include "support.h"
#include "utils/msg.h"

Test(ks_handshake, sends_the_module_id_over_a_live_socket)
{
  t_socket* peer;
  t_socket* socket = ks_connected_pair(&peer);
  t_log* logger = ks_quiet_logger();

  cr_assert(respond_handshake(socket, MID_KERNEL_SCHEDULER, logger));
  cr_assert_eq(receive_handshake(peer), MID_KERNEL_SCHEDULER);

  socket_destroy(socket);
  socket_destroy(peer);
  log_destroy(logger);
}

Test(ks_handshake, reports_failure_on_a_dead_socket)
{
  t_log* logger = ks_quiet_logger();
  cr_assert_not(respond_handshake(NULL, MID_KERNEL_SCHEDULER, logger));
  log_destroy(logger);
}

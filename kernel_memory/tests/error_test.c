#include "kernel_memory/error.h"

#include <criterion/criterion.h>

#include "support.h"
#include "utils/msg.h"

Test(km_error, send_handshake_error_destroys_the_socket)
{
  t_socket* peer;
  t_socket* socket = km_connected_pair(&peer);

  t_log* logger = km_quiet_logger();
  send_handshake_error(logger, socket, "Kernel Scheduler");

  cr_assert_eq(receive_op_code(peer), OP_CODE_ERROR,
               "the peer should see the socket closed");

  socket_destroy(peer);
  log_destroy(logger);
}

Test(km_error, send_init_error_closes_the_socket_but_leaves_it_to_its_owner)
{
  t_socket* peer;
  t_socket* socket = km_connected_pair(&peer);

  t_log* logger = km_quiet_logger();
  send_init_error(logger, socket, "Memory Stick");

  cr_assert_eq(receive_op_code(peer), OP_CODE_ERROR,
               "the peer should see the socket closed");

  socket_destroy(socket);
  socket_destroy(peer);
  log_destroy(logger);
}

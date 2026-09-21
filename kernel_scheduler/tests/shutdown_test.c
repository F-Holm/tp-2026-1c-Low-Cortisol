#include "kernel_scheduler/shutdown.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "support.h"
#include "utils/msg.h"

Test(ks_shutdown, notifies_kernel_memory_when_there_are_no_more_processes)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  close_kernel_scheduler(SR_NO_PROCESSES);

  cr_assert_eq(receive_op_code(server_fd), OP_KERNEL_SCHEDULER_SHUTDOWN);
  free(receive_string(server_fd));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown, is_idempotent)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  close_kernel_scheduler(SR_NO_PROCESSES);
  close_kernel_scheduler(SR_NO_PROCESSES);

  cr_assert_eq(receive_op_code(server_fd), OP_KERNEL_SCHEDULER_SHUTDOWN);
  free(receive_string(server_fd));
  /* the second call must not have sent anything else */
  socket_shutdown(client_fd, SOCKET_SHUTDOWN_WRITE);
  cr_assert_eq(receive_op_code(server_fd), OP_CODE_ERROR);

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown,
     resolves_a_send_error_by_treating_a_dead_peer_as_a_connection_failure)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  socket_destroy(server_fd); /* the peer is already gone */
  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  /* Regression guard: check_reason_shutdown used to block forever here
   * waiting for a reply nobody would ever send. */
  close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);

  socket_destroy(client_fd);
  log_destroy(logger);
}

Test(ks_shutdown, resolves_a_send_error_by_detecting_memory_corruption)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  /* An unrelated message first, to exercise the "keep draining" branch. */
  cr_assert(send_string(OP_KERNEL_MEMORY_RUNNING, "still alive", server_fd));
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  init_shutdown(NULL, logger, client_fd);

  close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown, shuts_down_the_server_socket_regardless_of_reason)
{
  t_socket* listen_peer_fd;
  /* Stands in for the accept-loop's server_socket. */
  t_socket* listen_side_fd = ks_connected_pair(&listen_peer_fd);

  t_socket* km_server_fd;
  t_socket* km_client_fd = ks_connected_pair(&km_server_fd);

  t_log* logger = ks_quiet_logger();
  init_shutdown(listen_side_fd, logger, km_client_fd);

  close_kernel_scheduler(SR_UNKNOWN_CAUSE);

  cr_assert_eq(receive_op_code(listen_peer_fd), OP_CODE_ERROR);

  socket_destroy(listen_side_fd);
  socket_destroy(listen_peer_fd);
  socket_destroy(km_client_fd);
  socket_destroy(km_server_fd);
  log_destroy(logger);
}

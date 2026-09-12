#include "kernel_scheduler/shutdown.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

Test(ks_shutdown, notifies_kernel_memory_when_there_are_no_more_processes)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  t_log* logger = ks_quiet_logger();

  close_kernel_scheduler(-1, logger, SR_NO_PROCESSES, client_fd);

  cr_assert_eq(receive_op_code(server_fd), OP_KERNEL_SCHEDULER_SHUTDOWN);
  free(receive_string(server_fd));

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown, is_idempotent)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  t_log* logger = ks_quiet_logger();

  close_kernel_scheduler(-1, logger, SR_NO_PROCESSES, client_fd);
  close_kernel_scheduler(-1, logger, SR_NO_PROCESSES, client_fd);

  cr_assert_eq(receive_op_code(server_fd), OP_KERNEL_SCHEDULER_SHUTDOWN);
  free(receive_string(server_fd));
  /* the second call must not have sent anything else */
  shutdown(client_fd, SHUT_WR);
  cr_assert_eq(receive_op_code(server_fd), OP_CODE_ERROR);

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown,
     resolves_a_send_error_by_treating_a_dead_peer_as_a_connection_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  close(server_fd); /* the peer is already gone */
  t_log* logger = ks_quiet_logger();

  /* Regression guard: check_reason_shutdown used to block forever here
   * waiting for a reply nobody would ever send. */
  close_kernel_scheduler(-1, logger, SR_KERNEL_MEMORY_SEND_ERROR, client_fd);

  close(client_fd);
  log_destroy(logger);
}

Test(ks_shutdown, resolves_a_send_error_by_detecting_memory_corruption)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  /* An unrelated message first, to exercise the "keep draining" branch. */
  cr_assert(send_string(OP_KERNEL_MEMORY_RUNNING, "still alive", server_fd));
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  close_kernel_scheduler(-1, logger, SR_KERNEL_MEMORY_SEND_ERROR, client_fd);

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_shutdown, shuts_down_the_server_socket_regardless_of_reason)
{
  int listen_peer_fd;
  /* Stands in for the accept-loop's server_socket. */
  int listen_side_fd = ks_connected_pair(&listen_peer_fd);

  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);

  t_log* logger = ks_quiet_logger();
  close_kernel_scheduler(listen_side_fd, logger, SR_UNKNOWN_CAUSE,
                         km_client_fd);

  char buf[1];
  cr_assert_eq(recv(listen_peer_fd, buf, sizeof(buf), 0), 0);

  close(listen_side_fd);
  close(listen_peer_fd);
  close(km_client_fd);
  close(km_server_fd);
  log_destroy(logger);
}

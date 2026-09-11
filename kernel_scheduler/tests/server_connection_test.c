#include <criterion/criterion.h>
#include <unistd.h>

#include "kernel_scheduler/connections/server.h"
#include "support.h"

Test(ks_server_connection, creates_a_listening_socket_on_an_ephemeral_port)
{
  t_log* logger = ks_quiet_logger();

  int socket_server = create_socket_server("0", logger);
  cr_assert_geq(socket_server, 0);

  close(socket_server);
  log_destroy(logger);
}

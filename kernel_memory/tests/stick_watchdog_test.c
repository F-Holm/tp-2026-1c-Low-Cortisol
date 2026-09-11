#include "kernel_memory/stick_watchdog.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

Test(km_stick_watchdog, starts_and_stops_cleanly_with_no_sticks_connected)
{
  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(-1, NULL, 0, 0, 1024, WORST, logger);

  t_stick_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);
  destroy_stick_watchdog(watchdog);

  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_stick_watchdog, notifies_the_scheduler_once_a_stick_goes_unreachable)
{
  int scheduler_server_fd;
  int scheduler_client_fd = km_connected_pair(&scheduler_server_fd);

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(-1, NULL, 0, 0, 1024, WORST, logger);
  kernel_data->socket_scheduler = scheduler_server_fd;

  t_stick_data* stick = km_make_stick(1024);
  stick->socket_stick = -1; /* pinging it always fails */
  list_add(kernel_data->connected_sticks, stick);

  t_stick_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(scheduler_client_fd), OP_MEMORY_CORRUPTED);
  free(receive_string(scheduler_client_fd));

  destroy_stick_watchdog(watchdog);

  close(scheduler_client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

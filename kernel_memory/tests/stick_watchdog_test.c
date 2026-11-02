#include "kernel_memory/stick_watchdog.h"

#include <criterion/criterion.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/time.h"

Test(km_stick_watchdog, starts_and_stops_cleanly_with_no_sticks_connected)
{
  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(nullptr, nullptr, 0, 0, 1024, AS_WORST, logger);

  t_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);
  destroy_stick_watchdog(watchdog);

  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_stick_watchdog, notifies_the_scheduler_once_a_stick_goes_unreachable)
{
  t_socket* scheduler_server_fd;
  t_socket* scheduler_client_fd = km_connected_pair(&scheduler_server_fd);

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(nullptr, nullptr, 0, 0, 1024, AS_WORST, logger);
  atomic_store(&(kernel_data->socket_scheduler), scheduler_server_fd);

  t_stick_data* stick = km_make_stick(1024);
  stick->socket_stick = nullptr; /* pinging it always fails */
  list_add(kernel_data->connected_sticks, stick);

  t_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(scheduler_client_fd), OP_MEMORY_CORRUPTED);
  free(receive_string(scheduler_client_fd));

  destroy_stick_watchdog(watchdog);

  socket_destroy(scheduler_client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

/* The stick watchdog pings every 500ms; 1.2s is a couple of full passes. */
#define TWO_WATCHDOG_PASSES_MS 1200

Test(km_stick_watchdog, notifies_the_scheduler_only_once_per_outage,
     .timeout = 10.0)
{
  t_socket* scheduler_server_fd;
  t_socket* scheduler_client_fd = km_connected_pair(&scheduler_server_fd);

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(nullptr, nullptr, 0, 0, 1024, AS_WORST, logger);
  atomic_store(&(kernel_data->socket_scheduler), scheduler_server_fd);

  t_stick_data* stick = km_make_stick(1024);
  stick->socket_stick = nullptr; /* pinging it always fails */
  list_add(kernel_data->connected_sticks, stick);

  t_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(scheduler_client_fd), OP_MEMORY_CORRUPTED);
  free(receive_string(scheduler_client_fd));

  /* later failing pings of the same stick stay silent */
  time_sleep_ms(TWO_WATCHDOG_PASSES_MS);
  cr_assert_not(socket_has_pending_data(scheduler_client_fd));

  destroy_stick_watchdog(watchdog);
  socket_destroy(scheduler_client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_stick_watchdog, stays_quiet_while_no_scheduler_is_connected,
     .timeout = 10.0)
{
  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(nullptr, nullptr, 0, 0, 1024, AS_WORST, logger);

  t_stick_data* stick = km_make_stick(1024);
  stick->socket_stick = nullptr;
  list_add(kernel_data->connected_sticks, stick);

  t_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);
  time_sleep_ms(TWO_WATCHDOG_PASSES_MS); /* failing pings, nobody to tell */

  destroy_stick_watchdog(watchdog);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_stick_watchdog, survives_failing_to_notify_the_scheduler,
     .timeout = 10.0)
{
  t_socket* scheduler_server_fd;
  t_socket* scheduler_client_fd = km_connected_pair(&scheduler_server_fd);
  socket_shutdown(scheduler_server_fd, SOCKET_SHUTDOWN_WRITE);

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(nullptr, nullptr, 0, 0, 1024, AS_WORST, logger);
  atomic_store(&(kernel_data->socket_scheduler), scheduler_server_fd);

  t_stick_data* stick = km_make_stick(1024);
  stick->socket_stick = nullptr;
  list_add(kernel_data->connected_sticks, stick);

  t_watchdog* watchdog = start_stick_watchdog(kernel_data);
  cr_assert_not_null(watchdog);
  time_sleep_ms(TWO_WATCHDOG_PASSES_MS); /* the notification send fails */
  cr_assert_eq(receive_op_code(scheduler_client_fd), OP_CODE_ERROR);

  destroy_stick_watchdog(watchdog);
  socket_destroy(scheduler_client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

#include "kernel_scheduler/scheduler/memory_query.h"

#include <criterion/criterion.h>
#include <stdbool.h>

#include "kernel_scheduler/scheduler/queue_types.h"
#include "support.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

/* ── space_available ───────────────────────────────────────────────────── */

Test(ks_memory_query, space_available_returns_the_reported_space)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 512;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;

  cr_assert_eq(space_available(queues, 1), 512);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, space_available_retries_after_a_new_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  int space = 700;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  /* create_resumption_routine_thread's spawned thread no-ops immediately
   * when terminate_routines is set -- isolates this test from the resumer
   * subsystem, which needs its own dedicated coverage. */
  queues->routines.terminate_routines = true;

  cr_assert_eq(space_available(queues, 1), 700);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, space_available_reports_a_send_failure)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  /* shutdown(SHUT_WR), not close(): a single small send() on a loopback
   * socket after the peer merely closes often still succeeds silently. */
  socket_shutdown(client_fd, SOCKET_SHUTDOWN_WRITE);
  socket_destroy(server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;

  cr_assert_eq(space_available(queues, 1), -1);

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── process_size ──────────────────────────────────────────────────────── */

Test(ks_memory_query, process_size_returns_the_reported_size)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int size = 256;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;

  cr_assert_eq(process_size(queues, 1), 256);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, process_size_no_logger_returns_the_reported_size)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int size = 128;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;

  cr_assert_eq(process_size_no_logger(queues, 1), 128);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, process_size_retries_after_a_new_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  int size = 300;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_ks_file_logger file_logger = ks_open_file_logger();
  t_queues* queues = ks_stub_queues_full(file_logger.logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;

  cr_assert_eq(process_size(queues, 1), 300);
  cr_assert(ks_file_logger_contains(&file_logger, "Process size: 300"));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  ks_close_file_logger(&file_logger);
}

Test(ks_memory_query,
     process_size_no_logger_stays_quiet_after_a_new_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  int size = 400;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_ks_file_logger file_logger = ks_open_file_logger();
  t_queues* queues = ks_stub_queues_full(file_logger.logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;

  cr_assert_eq(process_size_no_logger(queues, 1), 400);
  cr_assert_not(ks_file_logger_contains(&file_logger, "Process size"),
                "the no-logger variant must not log the size, even on a retry");

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  ks_close_file_logger(&file_logger);
}

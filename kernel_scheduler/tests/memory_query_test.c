#include "kernel_scheduler/scheduler/memory_query.h"

#include <criterion/criterion.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

/* ── space_available ───────────────────────────────────────────────────── */

Test(ks_memory_query, space_available_returns_the_reported_space)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 512;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  cr_assert_eq(space_available(queues, 1), 512);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, space_available_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  int space = 700;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  /* create_resumption_routine_thread's spawned thread no-ops immediately
   * when terminate_routines is set -- isolates this test from the resumer
   * subsystem, which needs its own dedicated coverage. */
  queues->terminate_routines = true;

  cr_assert_eq(space_available(queues, 1), 700);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, space_available_reports_a_send_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  close(server_fd); /* the send() now fails deterministically */

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  cr_assert_eq(space_available(queues, 1), -1);

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── process_size ──────────────────────────────────────────────────────── */

Test(ks_memory_query, process_size_returns_the_reported_size)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int size = 256;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  cr_assert_eq(process_size(queues, 1), 256);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_memory_query, process_size_no_logger_returns_the_reported_size)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int size = 128;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  cr_assert_eq(process_size_no_logger(queues, 1), 128);

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

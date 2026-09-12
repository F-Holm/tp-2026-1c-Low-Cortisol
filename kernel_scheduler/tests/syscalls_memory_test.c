#include <criterion/criterion.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/syscalls/memory.h"
#include "support.h"
#include "utils/msg.h"

/* ── allocate_memory ───────────────────────────────────────────────────── */

Test(ks_syscalls_memory, allocate_memory_fails_when_there_is_not_enough_space)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 10;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_a_send_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  /* shutdown(SHUT_WR), not close(): a single small send() on a loopback
   * socket after the peer merely closes often still succeeds silently. */
  shutdown(client_fd, SHUT_WR);
  close(server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_succeeds_when_kernel_memory_allocates)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_fails_when_the_segment_is_too_large)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_SEGMENT_SIZE_EXCEEDED, "too large for a stick",
                        server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_memory_corruption)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_an_unrecognized_reply)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_retries_after_compaction)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_COMPACTION_NEEDED, "compacting", server_fd));
  cr_assert(send_string(OP_COMPACTION_DONE, "done", server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

/* ── free_memory ───────────────────────────────────────────────────────── */

Test(ks_syscalls_memory, free_memory_reports_a_send_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  /* shutdown(SHUT_WR), not close(): a single small send() on a loopback
   * socket after the peer merely closes often still succeeds silently. */
  shutdown(client_fd, SHUT_WR);
  close(server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_succeeds_when_kernel_memory_frees)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_FREED, "freed", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines =
      true; /* isolates the always-on resumption kick */
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_reports_memory_corruption)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_reports_an_unrecognized_reply)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_MEMORY_FREED, "freed", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

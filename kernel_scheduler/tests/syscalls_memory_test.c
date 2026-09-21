#include <criterion/criterion.h>
#include <stdbool.h>

#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/scheduler/suspension.h"
#include "kernel_scheduler/syscalls/memory.h"
#include "support.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/syscalls.h"

/* ── allocate_memory ───────────────────────────────────────────────────── */

Test(ks_syscalls_memory, allocate_memory_fails_when_there_is_not_enough_space)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 10;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_a_send_failure)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  /* shutdown(SHUT_WR), not close(): a single small send() on a loopback
   * socket after the peer merely closes often still succeeds silently. */
  socket_shutdown(client_fd, SOCKET_SHUTDOWN_WRITE);
  socket_destroy(server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_succeeds_when_kernel_memory_allocates)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_fails_when_the_segment_is_too_large)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_SEGMENT_SIZE_EXCEEDED, "too large for a stick",
                        server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory,
     allocate_memory_fails_when_kernel_memory_reports_not_enough_memory)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_NOT_ENOUGH_MEMORY, "the space changed", server_fd));
  /* Whatever follows must still be readable: the failure reply's payload has
   * to be consumed, not left on the wire. */
  cr_assert(send_string(OP_MEMORY_FREED, "next message", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(allocate_memory(&request, queues));
  cr_assert_eq(receive_op_code(client_fd), OP_MEMORY_FREED);

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_memory_corruption)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_reports_an_unrecognized_reply)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_retries_after_compaction)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_COMPACTION_NEEDED, "compacting", server_fd));
  cr_assert(send_string(OP_COMPACTION_DONE, "done", server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  start_threads_suspended(queues, 60000);
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, allocate_memory_retries_after_a_new_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(allocate_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

/* ── free_memory ───────────────────────────────────────────────────────── */

Test(ks_syscalls_memory, free_memory_reports_a_send_failure)
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
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_succeeds_when_kernel_memory_frees)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_FREED, "freed", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines =
      true; /* isolates the always-on resumption kick */
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_reports_memory_corruption)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_reports_an_unrecognized_reply)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert_not(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_syscalls_memory, free_memory_retries_after_a_new_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_MEMORY_FREED, "freed", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  queues->routines.terminate_routines = true;
  t_syscall_memory request = {.pid = 1, .segment_id = 0, .size = 100};

  cr_assert(free_memory(&request, queues));

  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

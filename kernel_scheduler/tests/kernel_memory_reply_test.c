#include "kernel_scheduler/scheduler/kernel_memory_reply.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "kernel_scheduler/scheduler/queue_types.h"
#include "support.h"
#include "utils/msg.h"
#include "utils/sockets.h"

/* Every test wires a stub scheduler to a real loopback "Kernel Memory" peer and
 * uses a file logger to see whether the scheduler was shut down: each test
 * runs in its own process, so close_kernel_scheduler()'s once-only flag never
 * leaks between tests. */
typedef struct
{
  t_socket* km_peer;
  t_ks_file_logger file_logger;
  t_queues* queues;
} t_fixture;

static const char* const CORRUPTION_LOG = "BSOD: Corruption of memory detected";
static const char* const CONNECTION_LOG = "Connection error with Kernel Memory";

static t_fixture start_fixture(void)
{
  t_fixture f;
  t_socket* km_client = ks_connected_pair(&f.km_peer);
  f.file_logger = ks_open_file_logger();
  f.queues = ks_stub_queues_full(f.file_logger.logger);
  // The resumption routine a new memory stick starts is not under test here.
  f.queues->routines.terminate_routines = true;
  // Replace the stub's dead socket everywhere it was shared.
  socket_destroy(f.queues->km_socket);
  f.queues->km_socket = km_client;
  f.queues->process_counter->km_socket = km_client;
  return f;
}

static void end_fixture(t_fixture* f)
{
  ks_wait_thread_counter_zero(f->queues);
  ks_destroy_stub_queues_full(f->queues);
  socket_destroy(f->km_peer);
  ks_close_file_logger(&f->file_logger);
}

/* ── expected op codes ─────────────────────────────────────────────────── */

Test(ks_km_reply, returns_the_expected_op_code_and_leaves_its_payload_unread)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "the payload", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_MEMORY_ALLOCATED),
               OP_MEMORY_ALLOCATED);

  char* payload = receive_string(f.queues->km_socket);
  cr_assert_str_eq(payload, "the payload");
  cr_assert_not(ks_file_logger_contains(&f.file_logger, "Shutting down"));

  free(payload);
  end_fixture(&f);
}

Test(ks_km_reply, accepts_either_of_two_expected_op_codes)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_SUSPENSION_FAILED, "no swap", f.km_peer));
  cr_assert(send_string(OP_SUSPENSION_OK, "done", f.km_peer));

  cr_assert_eq(
      RECEIVE_KM_OPCODE(f.queues, OP_SUSPENSION_OK, OP_SUSPENSION_FAILED),
      OP_SUSPENSION_FAILED);
  free(receive_string(f.queues->km_socket));
  cr_assert_eq(
      RECEIVE_KM_OPCODE(f.queues, OP_SUSPENSION_OK, OP_SUSPENSION_FAILED),
      OP_SUSPENSION_OK);
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

Test(ks_km_reply, accepts_any_of_three_or_more_expected_op_codes)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_COMPACTION_NEEDED, "compact", f.km_peer));

  cr_assert_eq(
      RECEIVE_KM_OPCODE(f.queues, OP_MEMORY_ALLOCATED, OP_SEGMENT_SIZE_EXCEEDED,
                        OP_NOT_ENOUGH_MEMORY, OP_COMPACTION_NEEDED),
      OP_COMPACTION_NEEDED);
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

/* ── OP_NEW_MEMORY_STICK ───────────────────────────────────────────────── */

Test(ks_km_reply, skips_a_new_memory_stick_and_returns_the_expected_op_code)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", f.km_peer));
  cr_assert(send_string(OP_MEMORY_FREED, "freed", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_MEMORY_FREED), OP_MEMORY_FREED);

  char* payload = receive_string(f.queues->km_socket);
  cr_assert_str_eq(payload, "freed", "the stick's payload must be discarded");
  cr_assert_not(ks_file_logger_contains(&f.file_logger, "Shutting down"));

  free(payload);
  end_fixture(&f);
}

Test(ks_km_reply, skips_any_number_of_new_memory_sticks)
{
  t_fixture f = start_fixture();
  for (int i = 0; i < 500; i++)
    cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", f.km_peer));
  cr_assert(send_string(OP_PROCESS_STARTED, "started", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_PROCESS_STARTED),
               OP_PROCESS_STARTED);
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

Test(ks_km_reply, skips_a_new_memory_stick_between_two_expected_replies)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", f.km_peer));
  cr_assert(send_string(OP_RESUME_SUSPENSION_FAILED, "no room", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_RESUME_SUSPENSION_OK,
                                 OP_RESUME_SUSPENSION_FAILED),
               OP_RESUME_SUSPENSION_FAILED);
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

/* ── shutdown cases ────────────────────────────────────────────────────── */

Test(ks_km_reply, memory_corruption_shuts_down_and_returns_op_code_error)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", f.km_peer));
  cr_assert(send_string(OP_MEMORY_FREED, "next message", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_PROCESS_SIZE), OP_CODE_ERROR);

  cr_assert(ks_file_logger_contains(&f.file_logger, CORRUPTION_LOG));
  cr_assert_eq(receive_op_code(f.queues->km_socket), OP_MEMORY_FREED,
               "the corruption payload must be consumed");
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

Test(ks_km_reply, a_new_memory_stick_before_corruption_still_shuts_down)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", f.km_peer));
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_PROCESS_SIZE), OP_CODE_ERROR);

  cr_assert(ks_file_logger_contains(&f.file_logger, CORRUPTION_LOG));

  end_fixture(&f);
}

Test(ks_km_reply, an_unexpected_op_code_shuts_down_and_returns_op_code_error)
{
  t_fixture f = start_fixture();
  cr_assert(send_string(OP_FREE_MEMORY, "not what was asked", f.km_peer));
  cr_assert(send_string(OP_MEMORY_FREED, "next message", f.km_peer));

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_PROCESS_SIZE), OP_CODE_ERROR);

  cr_assert(ks_file_logger_contains(&f.file_logger, CONNECTION_LOG));
  cr_assert_not(ks_file_logger_contains(&f.file_logger, CORRUPTION_LOG));
  cr_assert_eq(receive_op_code(f.queues->km_socket), OP_MEMORY_FREED,
               "the unexpected payload must be consumed");
  free(receive_string(f.queues->km_socket));

  end_fixture(&f);
}

Test(ks_km_reply, a_closed_connection_shuts_down_and_returns_op_code_error)
{
  t_fixture f = start_fixture();
  socket_destroy(f.km_peer);
  f.km_peer = NULL;

  cr_assert_eq(RECEIVE_KM_OPCODE(f.queues, OP_PROCESS_SIZE), OP_CODE_ERROR);

  cr_assert(ks_file_logger_contains(&f.file_logger, CONNECTION_LOG));

  end_fixture(&f);
}

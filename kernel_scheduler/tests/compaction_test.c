#include "kernel_scheduler/scheduler/compaction.h"

#include <criterion/criterion.h>
#include <stdatomic.h>
#include <unistd.h>

#include "kernel_scheduler/domain/pcb.h"
#include "support.h"
#include "utils/msg.h"

/* ── is_compacting / is_resuming / terminate_routines ─────────────────── */

Test(ks_compaction, is_compacting_reflects_the_compaction_active_flag)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);

  cr_assert_not(is_compacting(queues));
  atomic_store(&(queues->compaction_active), true);
  cr_assert(is_compacting(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction, is_resuming_reflects_the_resume_active_flag)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);

  cr_assert_not(is_resuming(queues));
  atomic_store(&(queues->resume_active), true);
  cr_assert(is_resuming(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction, terminate_routines_sets_the_flag)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);

  cr_assert_not(queues->terminate_routines);
  terminate_routines(queues);
  cr_assert(queues->terminate_routines);

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── create_resumption_routine_thread ─────────────────────────────────── */

Test(ks_compaction,
     create_resumption_routine_thread_is_a_no_op_while_compacting)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  atomic_store(&(queues->compaction_active), true);

  create_resumption_routine_thread(queues);

  cr_assert_not(is_resuming(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction,
     create_resumption_routine_thread_is_a_no_op_while_already_resuming)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  atomic_store(&(queues->resume_active), true);

  create_resumption_routine_thread(queues);

  /* Still resuming, and no second thread was spawned to race it -- there's
   * nothing further to observe from here without reaching into internals,
   * so absence of a crash/hang under the isolated single-test timeout is
   * itself the assertion. */
  cr_assert(is_resuming(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction,
     create_resumption_routine_thread_spawns_and_finishes_cleanly)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  /* The spawned thread no-ops immediately once it sees terminate_routines,
   * isolating this test from the resumer subsystem's own logic (covered
   * separately in suspension_test.c). */
  queues->terminate_routines = true;

  create_resumption_routine_thread(queues);

  cr_assert(is_resuming(queues));
  ks_wait_thread_counter_zero(queues);

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── fits_process ──────────────────────────────────────────────────────── */

Test(ks_compaction, fits_process_true_when_kernel_memory_approves)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_RESUME_SUSPENSION_OK, "fits", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);

  cr_assert(fits_process(queues, pcb));

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, fits_process_false_when_kernel_memory_rejects)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_RESUME_SUSPENSION_FAILED, "no room", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);

  cr_assert_not(fits_process(queues, pcb));

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, fits_process_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_OK, "fits", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);

  cr_assert(fits_process(queues, pcb));

  destroy_pcb(pcb);
  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

/* ── routine_compaction ────────────────────────────────────────────────── */

Test(ks_compaction, routine_compaction_is_a_no_op_while_already_compacting)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  atomic_store(&(queues->compaction_active), true);

  routine_compaction(queues);

  cr_assert(is_compacting(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction,
     routine_compaction_skips_the_body_when_routines_are_terminated)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->terminate_routines = true;

  routine_compaction(queues);

  /* set_is_compacting(true) runs unconditionally before the
   * terminate_routines check, and the matching reset lives inside the
   * guarded body -- so a terminated routine set leaves the flag stuck. That
   * only matters mid-shutdown, when nothing reads it again. */
  cr_assert(is_compacting(queues));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_compaction, routine_compaction_completes_when_kernel_memory_finishes)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_COMPACTION_DONE, "done", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);

  routine_compaction(queues);

  cr_assert_not(is_compacting(queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, routine_compaction_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_COMPACTION_DONE, "done", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);

  routine_compaction(queues);

  cr_assert_not(is_compacting(queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, routine_compaction_shuts_down_on_memory_corruption)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);

  routine_compaction(queues);

  cr_assert_not(is_compacting(queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, routine_compaction_shuts_down_on_an_unrecognized_reply)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);

  routine_compaction(queues);

  cr_assert_not(is_compacting(queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_compaction, routine_compaction_recovers_from_a_send_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  close(server_fd); /* the send() now fails deterministically */

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  start_threads_suspended(queues, 60000);

  routine_compaction(queues);

  cr_assert_not(is_compacting(queues));

  ks_wait_thread_counter_zero(queues);
  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

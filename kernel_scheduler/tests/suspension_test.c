#include "kernel_scheduler/scheduler/suspension.h"

#include <criterion/criterion.h>
#include <stdatomic.h>
#include <unistd.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/* ── thread lifecycle ──────────────────────────────────────────────────── */

Test(ks_suspension, starts_and_stops_the_suspender_and_resumer_threads_cleanly)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);

  start_threads_suspended(queues, 1000);
  cr_assert_not_null(queues->suspension_data);

  terminate_threads_suspended(queues);
  cr_assert_eq(queues->suspension_data->suspender_thread_data->data->state,
               HS_FINISHED);
  cr_assert_eq(queues->suspension_data->resumer_thread_data->data->state,
               HS_FINISHED);

  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension, lock_threads_suspended_parks_both_worker_threads)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  start_threads_suspended(queues, 1000);

  lock_threads_suspended(queues);

  /* The workers may still be mid-iteration the instant we lock them; poll
   * briefly (bounded) rather than assume they're already parked. */
  t_suspended_thread* suspender_data =
      queues->suspension_data->suspender_thread_data->data;
  t_suspended_thread* resumer_data =
      queues->suspension_data->resumer_thread_data->data;
  bool suspender_blocked = false;
  bool resumer_blocked = false;
  for (int i = 0; i < 500 && !(suspender_blocked && resumer_blocked); i++)
  {
    pthread_mutex_lock(&(suspender_data->state_mutex));
    suspender_blocked = suspender_data->state == HS_BLOCKED;
    pthread_mutex_unlock(&(suspender_data->state_mutex));

    pthread_mutex_lock(&(resumer_data->state_mutex));
    resumer_blocked = resumer_data->state == HS_BLOCKED;
    pthread_mutex_unlock(&(resumer_data->state_mutex));

    if (!(suspender_blocked && resumer_blocked))
      usleep(1000);
  }
  cr_assert(suspender_blocked, "suspender thread never parked");
  cr_assert(resumer_blocked, "resumer thread never parked");

  unlock_threads_suspended(queues);

  terminate_threads_suspended(queues);
  destroy_threads_suspended(queues);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── transition_block_susp_block_no_mutex ─────────────────────────────── */

Test(ks_suspension, transition_block_susp_block_rejects_a_pcb_not_in_block)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_READY, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension, transition_block_susp_block_gives_up_on_a_notify_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  close(server_fd); /* the send() now fails deterministically */

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_BLOCK, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_BLOCK);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_block_susp_block_moves_the_pcb_when_kernel_memory_agrees)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_SUSPENSION_OK, "suspended", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  /* Skip the resumer-thread wakeup at the end -- that's the suspension
   * subsystem's own concern, already covered by the thread-lifecycle
   * tests above. */
  atomic_store(&(queues->resume_active), true);

  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));

  pthread_mutex_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_SUSP_BLOCK);
  cr_assert_eq(list_size(queues->block.list), 0);
  cr_assert_eq(list_size(queues->susp_block.list), 1);

  transition_take_susp_block(pcb, &(queues->susp_block));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

/* ── transition_susp_block_susp_ready_no_mutex ────────────────────────── */

Test(ks_suspension,
     transition_susp_block_susp_ready_rejects_a_pcb_not_in_susp_block)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_READY, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  transition_susp_block_susp_ready_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension, transition_susp_block_susp_ready_moves_the_pcb)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_SUSP_BLOCK, 0);
  transition_to_susp_block(pcb, &(queues->susp_block));

  pthread_mutex_lock(&(pcb->state_mutex));
  transition_susp_block_susp_ready_no_mutex(pcb, queues);
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_SUSP_READY);
  cr_assert_eq(list_size(queues->susp_block.list), 0);
  cr_assert_eq(list_size(queues->susp_ready.list), 1);

  transition_take_susp_ready(pcb, &(queues->susp_ready));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

/* ── transition_susp_ready_no_mutex ───────────────────────────────────── */

Test(ks_suspension, transition_susp_ready_rejects_a_pcb_not_in_susp_ready)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_READY, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  pthread_mutex_unlock(&(pcb->state_mutex));

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_susp_ready_declines_when_there_is_not_enough_space)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 100;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_SUSP_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_susp_ready_fails_when_kernel_memory_rejects_the_resume)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_FAILED, "no room", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);

  pthread_mutex_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_SUSP_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_suspension, transition_susp_ready_moves_the_pcb_back_to_ready)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_OK, "fits", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);
  transition_to_susp_ready(pcb, &(queues->susp_ready));

  pthread_mutex_lock(&(pcb->state_mutex));
  cr_assert(transition_susp_ready_no_mutex(pcb, queues));
  pthread_mutex_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(queues->susp_ready.list), 0);
  cr_assert_eq(transition_take_ready_next(&(queues->ready)), pcb);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

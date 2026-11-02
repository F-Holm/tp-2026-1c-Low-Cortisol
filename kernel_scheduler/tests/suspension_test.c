#include "kernel_scheduler/scheduler/suspension.h"

#include <criterion/criterion.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/time.h"

/* ── thread lifecycle ──────────────────────────────────────────────────── */

Test(ks_suspension, starts_and_stops_the_suspender_and_resumer_threads_cleanly)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);

  start_threads_suspended(queues, 1000);
  cr_assert_not_null(queues->routines.suspension_data);

  terminate_threads_suspended(queues);
  cr_assert_eq(
      queues->routines.suspension_data->suspender_thread_data->data->state,
      TS_FINISHED);
  cr_assert_eq(
      queues->routines.suspension_data->resumer_thread_data->data->state,
      TS_FINISHED);

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
      queues->routines.suspension_data->suspender_thread_data->data;
  t_suspended_thread* resumer_data =
      queues->routines.suspension_data->resumer_thread_data->data;
  bool suspender_blocked = false;
  bool resumer_blocked = false;
  for (int i = 0; i < 500 && !(suspender_blocked && resumer_blocked); i++)
  {
    mtx_lock(&(suspender_data->state_mutex));
    suspender_blocked = suspender_data->state == TS_BLOCKED;
    mtx_unlock(&(suspender_data->state_mutex));

    mtx_lock(&(resumer_data->state_mutex));
    resumer_blocked = resumer_data->state == TS_BLOCKED;
    mtx_unlock(&(resumer_data->state_mutex));

    if (!(suspender_blocked && resumer_blocked))
    {
      time_sleep_ms(1);
    }
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
  t_pcb* pcb = create_pcb(PS_READY, 0);

  mtx_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension, transition_block_susp_block_gives_up_on_a_notify_failure)
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
  t_pcb* pcb = create_pcb(PS_BLOCK, 0);

  mtx_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_BLOCK);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_block_susp_block_moves_the_pcb_when_kernel_memory_agrees)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_SUSPENSION_OK, "suspended", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  /* Skip the resumer-thread wakeup at the end -- that's the suspension
   * subsystem's own concern, already covered by the thread-lifecycle
   * tests above. */
  atomic_store(&(queues->routines.resume_active), true);

  t_pcb* pcb = create_pcb(PS_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));

  mtx_lock(&(pcb->state_mutex));
  transition_block_susp_block_no_mutex(pcb, queues);
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_SUSP_BLOCK);
  cr_assert_eq(list_size(queues->block.list), 0);
  cr_assert_eq(list_size(queues->susp_block.list), 1);

  transition_take_susp_block(pcb, &(queues->susp_block));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

/* ── transition_susp_block_susp_ready_no_mutex ────────────────────────── */

Test(ks_suspension,
     transition_susp_block_susp_ready_rejects_a_pcb_not_in_susp_block)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(PS_READY, 0);

  mtx_lock(&(pcb->state_mutex));
  transition_susp_block_susp_ready_no_mutex(pcb, queues);
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension, transition_susp_block_susp_ready_moves_the_pcb)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(PS_SUSP_BLOCK, 0);
  transition_to_susp_block(pcb, &(queues->susp_block));

  mtx_lock(&(pcb->state_mutex));
  transition_susp_block_susp_ready_no_mutex(pcb, queues);
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_SUSP_READY);
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
  t_pcb* pcb = create_pcb(PS_READY, 0);

  mtx_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  mtx_unlock(&(pcb->state_mutex));

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_susp_ready_declines_when_there_is_not_enough_space)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 100;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_pcb* pcb = create_pcb(PS_SUSP_READY, 0);

  mtx_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_SUSP_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_suspension,
     transition_susp_ready_fails_when_kernel_memory_rejects_the_resume)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_FAILED, "no room", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_pcb* pcb = create_pcb(PS_SUSP_READY, 0);

  mtx_lock(&(pcb->state_mutex));
  cr_assert_not(transition_susp_ready_no_mutex(pcb, queues));
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_SUSP_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

Test(ks_suspension, transition_susp_ready_moves_the_pcb_back_to_ready)
{
  t_socket* server_fd;
  t_socket* client_fd = ks_connected_pair(&server_fd);
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 500;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_OK, "fits", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = client_fd;
  t_pcb* pcb = create_pcb(PS_SUSP_READY, 0);
  transition_to_susp_ready(pcb, &(queues->susp_ready));

  mtx_lock(&(pcb->state_mutex));
  cr_assert(transition_susp_ready_no_mutex(pcb, queues));
  mtx_unlock(&(pcb->state_mutex));

  cr_assert_eq(pcb->state, PS_READY);
  cr_assert_eq(list_size(queues->susp_ready.list), 0);
  cr_assert_eq(transition_take_ready_next(&(queues->ready)), pcb);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  socket_destroy(server_fd);
  log_destroy(logger);
}

/* ── the suspender and resumer worker threads ─────────────────────────── */

/* A Kernel Memory that answers whatever the worker threads ask, for as long
 * as the connection stays open. */
typedef struct
{
  t_socket* socket;
  int process_size; /* answer to OP_REQUEST_PROCESS_SIZE */
  int free_memory;  /* answer to OP_REQUEST_FREE_MEMORY */
  bool resume_fits; /* answer to OP_RESUME_SUSPENDED_PROCESS */
} t_km_responder;

static int km_responder_thread(void* arg)
{
  t_km_responder* km = arg;
  while (true)
  {
    int op_code = receive_op_code(km->socket);
    int size;
    switch (op_code)
    {
      case OP_REQUEST_PROCESS_SIZE:
        free(receive_buffer(&size, km->socket));
        send_buffer(OP_PROCESS_SIZE, &km->process_size, sizeof(int),
                    km->socket);
        break;
      case OP_REQUEST_FREE_MEMORY:
        free(receive_string(km->socket));
        send_buffer(OP_FREE_MEMORY, &km->free_memory, sizeof(int), km->socket);
        break;
      case OP_SUSPEND_PROCESS:
        free(receive_buffer(&size, km->socket));
        send_string(OP_SUSPENSION_OK, "suspended", km->socket);
        break;
      case OP_RESUME_SUSPENDED_PROCESS:
        free(receive_buffer(&size, km->socket));
        send_string(km->resume_fits ? OP_RESUME_SUSPENSION_OK
                                    : OP_RESUME_SUSPENSION_FAILED,
                    "resume", km->socket);
        break;
      default: /* OP_CODE_ERROR: the scheduler side went away */
        return 0;
    }
  }
}

typedef struct
{
  t_log* logger;
  t_queues* queues;
  t_socket* km_peer;
  t_km_responder km;
  thrd_t km_thread;
} t_workers_fixture;

static void workers_fixture_start(t_workers_fixture* fx, int process_size,
                                  int free_memory, bool resume_fits)
{
  fx->logger = ks_quiet_logger();
  fx->queues = ks_stub_queues_full(fx->logger);
  fx->queues->km_socket = ks_connected_pair_with_mutex(&fx->km_peer);
  fx->km = (t_km_responder){.socket = fx->km_peer,
                            .process_size = process_size,
                            .free_memory = free_memory,
                            .resume_fits = resume_fits};
  thrd_create(&fx->km_thread, km_responder_thread, &fx->km);
  start_threads_suspended(fx->queues, 30); /* suspend after 30ms blocked */
}

/* Stops the worker threads and the fake Kernel Memory; the queues stay
 * alive so the test can take its PCBs back out of them. */
static void workers_fixture_stop(t_workers_fixture* fx)
{
  terminate_threads_suspended(fx->queues);
  destroy_threads_suspended(fx->queues);
  socket_shutdown(fx->queues->km_socket, SOCKET_SHUTDOWN_BOTH);
  thrd_join(fx->km_thread, nullptr);
  socket_destroy(fx->km_peer);
}

static void workers_fixture_free(t_workers_fixture* fx)
{
  ks_destroy_stub_queues_full(fx->queues);
  log_destroy(fx->logger);
}

/* Polls @p pcb's state (bounded) until it becomes @p expected. */
static bool wait_for_state(t_pcb* pcb, int expected, int timeout_ms)
{
  for (int waited = 0; waited < timeout_ms; waited += 5)
  {
    mtx_lock(&(pcb->state_mutex));
    bool reached = pcb->state == expected;
    mtx_unlock(&(pcb->state_mutex));
    if (reached)
    {
      return true;
    }
    time_sleep_ms(5);
  }
  return false;
}

Test(ks_suspension, the_suspender_suspends_a_process_blocked_past_the_timeout,
     .timeout = 10.0)
{
  t_workers_fixture fx;
  workers_fixture_start(&fx, 64, 0, false);

  t_pcb* pcb = create_pcb(PS_BLOCK, 0);
  transition_to_block(pcb, &(fx.queues->block));

  /* blocked for less than the timeout at first: the suspender waits it out
   * and suspends it on a later pass */
  cr_assert(wait_for_state(pcb, PS_SUSP_BLOCK, 3000));
  cr_assert_eq(list_size(fx.queues->block.list), 0);

  workers_fixture_stop(&fx);
  transition_take_susp_block(pcb, &(fx.queues->susp_block));
  workers_fixture_free(&fx);
  destroy_pcb(pcb);
}

Test(ks_suspension, the_suspender_skips_a_process_with_nothing_in_memory,
     .timeout = 10.0)
{
  t_workers_fixture fx;
  workers_fixture_start(&fx, 0, 0, false); /* nothing to move to swap */

  t_pcb* pcb = create_pcb(PS_BLOCK, 0);
  transition_to_block(pcb, &(fx.queues->block));

  cr_assert_not(wait_for_state(pcb, PS_SUSP_BLOCK, 200));

  workers_fixture_stop(&fx);
  transition_take_block(pcb, &(fx.queues->block));
  workers_fixture_free(&fx);
  destroy_pcb(pcb);
}

Test(ks_suspension, the_resumer_brings_a_suspended_process_back_to_ready,
     .timeout = 10.0)
{
  t_workers_fixture fx;
  workers_fixture_start(&fx, 64, 1024, true);

  t_pcb* pcb = create_pcb(PS_SUSP_READY, 0);
  transition_to_susp_ready(pcb, &(fx.queues->susp_ready));

  cr_assert(wait_for_state(pcb, PS_READY, 3000));
  cr_assert_eq(list_size(fx.queues->susp_ready.list), 0);

  workers_fixture_stop(&fx);
  cr_assert_eq(transition_take_ready_next(&(fx.queues->ready)), pcb);
  workers_fixture_free(&fx);
  destroy_pcb(pcb);
}

Test(ks_suspension, the_resumer_parks_itself_when_the_process_does_not_fit,
     .timeout = 10.0)
{
  t_workers_fixture fx;
  workers_fixture_start(&fx, 64, 1024, false); /* Kernel Memory says no */

  t_pcb* pcb = create_pcb(PS_SUSP_READY, 0);
  transition_to_susp_ready(pcb, &(fx.queues->susp_ready));

  t_suspended_thread* resumer =
      fx.queues->routines.suspension_data->resumer_thread_data->data;
  bool parked = false;
  for (int waited = 0; waited < 3000 && !parked; waited += 5)
  {
    mtx_lock(&(resumer->state_mutex));
    parked = resumer->state == TS_BLOCKED;
    mtx_unlock(&(resumer->state_mutex));
    time_sleep_ms(5);
  }
  cr_assert(parked, "the resumer should wait for memory to free up");
  cr_assert_eq(pcb->state, PS_SUSP_READY);

  workers_fixture_stop(&fx);
  transition_take_susp_ready(pcb, &(fx.queues->susp_ready));
  workers_fixture_free(&fx);
  destroy_pcb(pcb);
}

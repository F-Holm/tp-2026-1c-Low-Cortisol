#include "kernel_scheduler/scheduler/queues.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/domain/pcb.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

static t_list* levels(int count, int algo)
{
  t_list* l = list_create();
  for (int i = 0; i < count; i++)
  {
    int* a = malloc(sizeof(int));
    *a = algo;
    list_add(l, a);
  }
  return l;
}

/* ── init_queues / destroy_queues ─────────────────────────────────────── */

Test(ks_queues, init_queues_builds_a_working_full_lifecycle)
{
  t_log* logger = ks_quiet_logger();
  t_kernel_memory_socket* km_socket = init_socket_kernel_memory(-1);

  t_queues* queues =
      init_queues(AP_FIFO, NULL, 0, false, -1, logger, km_socket, 1000);

  cr_assert_not_null(queues);
  cr_assert_eq(queues->logger, logger);
  cr_assert_eq(queues->km_socket, km_socket);
  cr_assert_not_null(queues->suspension_data);
  cr_assert_not(queues->terminate_routines);

  /* destroy_queues waits out the suspender/resumer threads itself. */
  destroy_queues(queues);
  destroy_kernel_memory(km_socket);
  log_destroy(logger);
}

/* ── transition_new_ready ──────────────────────────────────────────────── */

Test(ks_queues, transition_new_ready_moves_the_process_to_ready)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_PROCESS_STARTED, "started", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  transition_new_ready(queues, "a.txt", 0);

  t_pcb* pcb = transition_take_ready_next(&(queues->ready));
  cr_assert_not_null(pcb);
  cr_assert_eq(pcb->state, EST_READY);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_queues, transition_new_ready_reports_a_send_failure)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  shutdown(client_fd, SHUT_WR);
  close(server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  transition_new_ready(queues, "a.txt", 0);

  cr_assert_null(transition_take_ready_next(&(queues->ready)));

  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_queues, transition_new_ready_gives_up_on_memory_corruption)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_CORRUPTED, "boom", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  transition_new_ready(queues, "a.txt", 0);

  cr_assert_null(transition_take_ready_next(&(queues->ready)));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_queues, transition_new_ready_gives_up_on_an_unrecognized_reply)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;

  transition_new_ready(queues, "a.txt", 0);

  cr_assert_null(transition_take_ready_next(&(queues->ready)));

  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_queues, transition_new_ready_retries_after_a_new_memory_stick)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_NEW_MEMORY_STICK, "a stick connected", server_fd));
  cr_assert(send_string(OP_PROCESS_STARTED, "started", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  queues->terminate_routines = true;

  transition_new_ready(queues, "a.txt", 0);

  t_pcb* pcb = transition_take_ready_next(&(queues->ready));
  cr_assert_not_null(pcb);

  destroy_pcb(pcb);
  ks_wait_thread_counter_zero(queues);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_queues, transition_new_ready_exits_a_process_with_an_invalid_priority)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_PROCESS_STARTED, "started", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  destroy_ready_queue(&(queues->ready));
  t_list* algos = levels(2, AP_FIFO);
  init_ready_queue(&(queues->ready), AP_CMN, algos);

  transition_new_ready(queues, "a.txt", 5); /* past the last level */

  cr_assert_null(transition_take_ready_next(&(queues->ready)));

  ks_destroy_stub_queues_full(queues);
  list_destroy_and_destroy_elements(algos, free);
  close(server_fd);
  log_destroy(logger);
}

/* ── update_priority ───────────────────────────────────────────────────── */

Test(ks_queues, update_priority_is_a_no_op_for_a_non_multilevel_queue)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_READY, 0);
  transition_to_ready(pcb, &(queues->ready));

  pcb->priority = 3; /* would be out of range if this were multilevel */
  update_priority(pcb, queues);

  cr_assert_eq(transition_take_ready_next(&(queues->ready)), pcb);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_queues, update_priority_is_a_no_op_for_a_pcb_not_in_ready)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  destroy_ready_queue(&(queues->ready));
  t_list* algos = levels(3, AP_FIFO);
  init_ready_queue(&(queues->ready), AP_CMN, algos);
  t_pcb* pcb = create_pcb(EST_BLOCK, 0);

  update_priority(pcb, queues);

  cr_assert_null(transition_take_ready_next(&(queues->ready)));

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  list_destroy_and_destroy_elements(algos, free);
  log_destroy(logger);
}

Test(ks_queues, update_priority_re_sorts_a_ready_pcb_into_its_new_level)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  destroy_ready_queue(&(queues->ready));
  t_list* algos = levels(3, AP_FIFO);
  init_ready_queue(&(queues->ready), AP_CMN, algos);
  t_pcb* pcb = create_pcb(EST_READY, 0);
  transition_to_ready(pcb, &(queues->ready));

  pcb->priority = 2;
  update_priority(pcb, queues);

  cr_assert_eq(transition_take_ready_next(&(queues->ready)), pcb);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  list_destroy_and_destroy_elements(algos, free);
  log_destroy(logger);
}

/* ── mutex-taking wrappers around the *_no_mutex transitions ──────────────
 * The _no_mutex core logic (called with pcb->state_mutex already held) is
 * covered in depth by suspension_test.c; these just confirm the public
 * wrapper actually takes the lock, delegates, and releases it. */

Test(ks_queues, transition_block_susp_block_moves_the_pcb)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_SUSPENSION_OK, "suspended", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  atomic_store(&(queues->resume_active), true); /* skip the resumer wakeup */

  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));

  transition_block_susp_block(pcb, queues);

  cr_assert_eq(pcb->state, EST_SUSP_BLOCK);
  cr_assert_eq(list_size(queues->susp_block.list), 1);

  transition_take_susp_block(pcb, &(queues->susp_block));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_queues, transition_susp_block_susp_ready_moves_the_pcb)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_pcb* pcb = create_pcb(EST_SUSP_BLOCK, 0);
  transition_to_susp_block(pcb, &(queues->susp_block));

  transition_susp_block_susp_ready(pcb, queues);

  cr_assert_eq(pcb->state, EST_SUSP_READY);
  cr_assert_eq(list_size(queues->susp_ready.list), 1);

  transition_take_susp_ready(pcb, &(queues->susp_ready));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_queues, transition_susp_ready_moves_the_pcb_back_to_ready)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  /* can_resume_suspended() makes two separate Kernel Memory round-trips
   * (available space, then this process's size) before
   * notify_process_resume_suspended() makes a third. */
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), server_fd));
  int size = 10;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), server_fd));
  cr_assert(send_string(OP_RESUME_SUSPENSION_OK, "resumed", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = client_fd;
  t_pcb* pcb = create_pcb(EST_SUSP_READY, 0);
  transition_to_susp_ready(pcb, &(queues->susp_ready));

  cr_assert(transition_susp_ready(pcb, queues));

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(queues->susp_ready.list), 0);

  transition_take_ready_next(&(queues->ready));
  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

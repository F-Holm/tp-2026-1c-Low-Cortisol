#include <criterion/criterion.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "support.h"

static t_queues* q;

static void setup(void) { q = ks_stub_queues_full(ks_quiet_logger()); }
static void teardown(void)
{
  log_destroy(q->logger);
  ks_destroy_stub_queues_full(q);
}

TestSuite(ks_state_machine, .init = setup, .fini = teardown);

Test(ks_state_machine, exec_to_ready_moves_the_pcb_between_the_queues)
{
  t_pcb* pcb = create_pcb(EST_EXEC, 0);
  transition_to_exec(pcb, &(q->exec));

  transition_exec_ready(pcb, q);

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(q->exec.list), 0);
  cr_assert_eq(transition_take_ready_next(&(q->ready)), pcb);

  destroy_pcb(pcb);
}

Test(ks_state_machine, exec_to_block_moves_the_pcb_to_block)
{
  t_pcb* pcb = create_pcb(EST_EXEC, 0);
  transition_to_exec(pcb, &(q->exec));

  transition_exec_block(pcb, q);

  cr_assert_eq(pcb->state, EST_BLOCK);
  cr_assert_eq(list_size(q->exec.list), 0);
  cr_assert_eq(transition_take_block_next(&(q->block)), pcb);

  destroy_pcb(pcb);
}

Test(ks_state_machine, a_transition_from_the_wrong_state_is_rejected)
{
  t_pcb* pcb = create_pcb(EST_READY, 0);
  transition_to_exec(pcb, &(q->exec)); /* physically in exec, state READY */

  transition_exec_block(pcb, q); /* expects EST_EXEC -> refused */

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(q->exec.list), 1, "pcb stays where it was");

  transition_take_exec_next(&(q->exec));
  destroy_pcb(pcb);
}

Test(ks_state_machine, block_to_ready_round_trip)
{
  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(q->block));

  transition_block_ready(pcb, q);

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert(blocking_list_is_empty(&(q->block)));
  cr_assert_eq(transition_take_ready_next(&(q->ready)), pcb);

  destroy_pcb(pcb);
}

Test(ks_state_machine, ready_to_exec_only_flips_the_state)
{
  t_pcb* pcb = create_pcb(EST_READY, 0);
  transition_ready_exec(pcb, q);
  cr_assert_eq(pcb->state, EST_EXEC);
  destroy_pcb(pcb);
}

Test(ks_state_machine, exec_to_exit_on_shutdown_drops_the_process)
{
  increment_process_count(q->process_counter);
  increment_process_count(q->process_counter);
  t_pcb* pcb = create_pcb(EST_EXEC, 0);
  transition_to_exec(pcb, &(q->exec));

  transition_exec_exit(pcb, q, PER_SYSTEM_SHUTDOWN); /* frees pcb */

  cr_assert_eq(list_size(q->exec.list), 0);
  cr_assert_eq(atomic_load(&(q->process_counter->active_process_count)), 1);
}

Test(ks_state_machine, clear_queues_drains_every_queue)
{
  increment_process_count(q->process_counter);
  increment_process_count(q->process_counter);
  increment_process_count(q->process_counter);

  t_pcb* r = create_pcb(EST_READY, 0);
  t_pcb* b = create_pcb(EST_BLOCK, 0);
  t_pcb* s = create_pcb(EST_SUSP_READY, 0);
  transition_to_ready(r, &(q->ready));
  transition_to_block(b, &(q->block));
  transition_to_susp_ready(s, &(q->susp_ready));

  clear_queues(q); /* frees r, b, s */

  cr_assert(is_queue_ready_empty(&(q->ready)));
  cr_assert(blocking_list_is_empty(&(q->block)));
  cr_assert(blocking_list_is_empty(&(q->susp_ready)));
  cr_assert_eq(atomic_load(&(q->process_counter->active_process_count)), 0);
}

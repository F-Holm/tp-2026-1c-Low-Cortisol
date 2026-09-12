#include "kernel_scheduler/scheduler/blocking_list.h"

#include <criterion/criterion.h>

#include "kernel_scheduler/domain/pcb.h"
#include "utils/collections/list.h"

Test(ks_blocking_list, starts_empty)
{
  t_blocking_list b;
  init_blocking_list(&b);
  cr_assert(blocking_list_is_empty(&b));
  cr_assert_not(b.new_process);
  destroy_blocking_list(&b);
}

Test(ks_blocking_list, to_block_stamps_the_time_and_raises_the_new_process_flag)
{
  t_blocking_list b;
  init_blocking_list(&b);
  t_pcb* pcb = create_pcb(EST_EXEC, 0);
  pcb->blocked_time = 0;

  transition_to_block(pcb, &b);

  cr_assert_not(blocking_list_is_empty(&b));
  cr_assert(b.new_process);
  cr_assert_neq(pcb->blocked_time, 0, "blocked_time should be set to now");

  destroy_pcb(pcb);
  destroy_blocking_list(&b);
}

Test(ks_blocking_list, take_block_clears_the_blocked_time)
{
  t_blocking_list b;
  init_blocking_list(&b);
  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &b);

  transition_take_block(pcb, &b);

  cr_assert(blocking_list_is_empty(&b));
  cr_assert_eq(pcb->blocked_time, 0);

  destroy_pcb(pcb);
  destroy_blocking_list(&b);
}

Test(ks_blocking_list, take_block_next_is_fifo_and_returns_null_when_empty)
{
  t_blocking_list b;
  init_blocking_list(&b);
  t_pcb* a = create_pcb(EST_BLOCK, 0);
  t_pcb* c = create_pcb(EST_BLOCK, 0);
  transition_to_block(a, &b);
  transition_to_block(c, &b);

  cr_assert_eq(transition_take_block_next(&b), a);
  cr_assert_eq(transition_take_block_next(&b), c);
  cr_assert_null(transition_take_block_next(&b));

  destroy_pcb(a);
  destroy_pcb(c);
  destroy_blocking_list(&b);
}

Test(ks_blocking_list, susp_block_is_kept_sorted_by_priority)
{
  t_blocking_list b;
  init_blocking_list(&b);
  t_pcb* low = create_pcb(EST_SUSP_BLOCK, 5);
  t_pcb* high = create_pcb(EST_SUSP_BLOCK, 1);
  t_pcb* mid = create_pcb(EST_SUSP_BLOCK, 3);

  transition_to_susp_block(low, &b);
  transition_to_susp_block(high, &b);
  transition_to_susp_block(mid, &b);

  cr_assert_eq(transition_take_susp_block_next(&b), high);
  cr_assert_eq(transition_take_susp_block_next(&b), mid);
  cr_assert_eq(transition_take_susp_block_next(&b), low);

  destroy_pcb(low);
  destroy_pcb(high);
  destroy_pcb(mid);
  destroy_blocking_list(&b);
}

Test(ks_blocking_list,
     susp_ready_is_kept_sorted_and_take_removes_a_specific_pcb)
{
  t_blocking_list b;
  init_blocking_list(&b);
  t_pcb* low = create_pcb(EST_SUSP_READY, 5);
  t_pcb* high = create_pcb(EST_SUSP_READY, 1);
  transition_to_susp_ready(low, &b);
  transition_to_susp_ready(high, &b);

  transition_take_susp_ready(high, &b);
  cr_assert_eq(transition_take_susp_ready_next(&b), low);
  cr_assert_null(transition_take_susp_ready_next(&b));

  destroy_pcb(low);
  destroy_pcb(high);
  destroy_blocking_list(&b);
}

#include "kernel_scheduler/scheduler/ready_queue.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "kernel_scheduler/domain/pcb.h"
#include "utils/collections/list.h"

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

/* ── init ─────────────────────────────────────────────────────────────── */

Test(ks_ready_queue, init_fifo_is_a_single_level)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  cr_assert_not(ready.multilevel_queue);
  cr_assert_eq(ready.queue_count, 1);
  cr_assert(is_queue_ready_empty(&ready));
  cr_assert_not(is_queue_ready_blocked(&ready));
  cr_assert_not(queue_ready_terminated(&ready));
  destroy_ready_queue(&ready);
}

Test(ks_ready_queue, init_cmn_makes_one_level_per_entry)
{
  t_list* algos = levels(4, AP_FIFO);
  t_ready_queue ready;
  init_ready_queue(&ready, AP_CMN, algos);
  cr_assert(ready.multilevel_queue);
  cr_assert_eq(ready.queue_count, 4);
  destroy_ready_queue(&ready);
  list_destroy_and_destroy_elements(algos, free);
}

/* ── gates ────────────────────────────────────────────────────────────── */

Test(ks_ready_queue, lock_and_unlock_toggle_the_preempt_flag)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  lock_queue_ready(&ready);
  cr_assert(is_queue_ready_blocked(&ready));
  unlock_queue_ready(&ready);
  cr_assert_not(is_queue_ready_blocked(&ready));
  destroy_ready_queue(&ready);
}

Test(ks_ready_queue, terminate_is_sticky_and_makes_blocking_take_return_null)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  terminate_queue_ready(&ready);
  cr_assert(queue_ready_terminated(&ready));
  cr_assert_null(transition_take_ready_blocking(&ready));
  destroy_ready_queue(&ready);
}

/* ── check_priority_valid ─────────────────────────────────────────────── */

Test(ks_ready_queue, single_level_accepts_any_priority)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  t_pcb* pcb = create_pcb(EST_READY, 99);
  cr_assert(check_priority_valid(pcb, &ready));
  destroy_pcb(pcb);
  destroy_ready_queue(&ready);
}

Test(ks_ready_queue, multilevel_rejects_a_priority_past_the_last_level)
{
  t_list* algos = levels(3, AP_FIFO);
  t_ready_queue ready;
  init_ready_queue(&ready, AP_CMN, algos);
  t_pcb* in = create_pcb(EST_READY, 2);
  t_pcb* out = create_pcb(EST_READY, 3);
  cr_assert(check_priority_valid(in, &ready));
  cr_assert_not(check_priority_valid(out, &ready));
  destroy_pcb(in);
  destroy_pcb(out);
  destroy_ready_queue(&ready);
  list_destroy_and_destroy_elements(algos, free);
}

/* ── FIFO put / take ──────────────────────────────────────────────────── */

Test(ks_ready_queue, fifo_take_next_returns_processes_in_arrival_order)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  t_pcb* a = create_pcb(EST_READY, 0);
  t_pcb* b = create_pcb(EST_READY, 0);

  transition_to_ready(a, &ready);
  transition_to_ready(b, &ready);
  cr_assert_not(is_queue_ready_empty(&ready));

  cr_assert_eq(transition_take_ready_next(&ready), a);
  cr_assert_eq(transition_take_ready_next(&ready), b);
  cr_assert_null(transition_take_ready_next(&ready));
  cr_assert(is_queue_ready_empty(&ready));

  destroy_pcb(a);
  destroy_pcb(b);
  destroy_ready_queue(&ready);
}

Test(ks_ready_queue, take_ready_removes_a_specific_process)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  t_pcb* a = create_pcb(EST_READY, 0);
  t_pcb* b = create_pcb(EST_READY, 0);
  transition_to_ready(a, &ready);
  transition_to_ready(b, &ready);

  transition_take_ready(a, &ready);
  cr_assert_eq(transition_take_ready_next(&ready), b);

  destroy_pcb(a);
  destroy_pcb(b);
  destroy_ready_queue(&ready);
}

/* ── multilevel ordering ─────────────────────────────────────────────── */

Test(ks_ready_queue, multilevel_serves_the_highest_priority_level_first)
{
  t_list* algos = levels(3, AP_FIFO);
  t_ready_queue ready;
  init_ready_queue(&ready, AP_CMN, algos);

  t_pcb* low = create_pcb(EST_READY, 2);
  t_pcb* high = create_pcb(EST_READY, 0);
  transition_to_ready(low, &ready);
  transition_to_ready(high, &ready);

  cr_assert_eq(ready.highest_priority, 0);
  cr_assert_eq(transition_take_ready_next(&ready), high);
  cr_assert_eq(transition_take_ready_next(&ready), low);

  destroy_pcb(low);
  destroy_pcb(high);
  destroy_ready_queue(&ready);
  list_destroy_and_destroy_elements(algos, free);
}

Test(ks_ready_queue, blocking_take_returns_a_ready_process_without_blocking)
{
  t_ready_queue ready;
  init_ready_queue(&ready, AP_FIFO, NULL);
  t_pcb* a = create_pcb(EST_READY, 0);
  transition_to_ready(a, &ready);

  cr_assert_eq(transition_take_ready_blocking(&ready), a);

  destroy_pcb(a);
  destroy_ready_queue(&ready);
}

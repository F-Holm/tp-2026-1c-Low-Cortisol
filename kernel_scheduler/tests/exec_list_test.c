#include "kernel_scheduler/scheduler/exec_list.h"

#include <criterion/criterion.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/counter.h"
#include "utils/collections/list.h"

Test(ks_exec_list, init_records_quantum_and_preemption)
{
  t_execute_list exec;
  init_exec_list(&exec, 50, true);
  cr_assert_eq(exec.quantum, 50);
  cr_assert(exec.preemption);
  cr_assert_eq(list_size(exec.list), 0);
  destroy_exec_list(&exec);
}

Test(ks_exec_list, put_and_take_a_specific_process)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, false);
  t_counter* syscalls = create_counter();
  t_pcb* a = create_pcb(EST_EXEC, 0);
  t_pcb* b = create_pcb(EST_EXEC, 0);

  transition_to_exec(a, &exec);
  transition_to_exec(b, &exec);
  cr_assert_eq(list_size(exec.list), 2);

  transition_take_exec(a, &exec, syscalls);
  cr_assert_eq(list_size(exec.list), 1);
  cr_assert_eq(transition_take_exec_next(&exec), b);

  destroy_pcb(a);
  destroy_pcb(b);
  destroy_counter(syscalls);
  destroy_exec_list(&exec);
}

Test(ks_exec_list, take_exec_next_returns_null_when_empty)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, false);
  cr_assert_null(transition_take_exec_next(&exec));
  destroy_exec_list(&exec);
}

Test(ks_exec_list, with_preemption_tracks_the_lowest_running_priority)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, true);
  t_counter* syscalls = create_counter();
  t_pcb* p1 = create_pcb(EST_EXEC, 1);
  t_pcb* p4 = create_pcb(EST_EXEC, 4);
  t_pcb* p2 = create_pcb(EST_EXEC, 2);

  transition_to_exec(p1, &exec);
  transition_to_exec(p4, &exec);
  transition_to_exec(p2, &exec);
  cr_assert_eq(exec.lowest_priority, 4);

  transition_take_exec(p4, &exec, syscalls);
  cr_assert_eq(exec.lowest_priority, 2, "recomputed after the p4 left");

  destroy_pcb(p1);
  destroy_pcb(p2);
  destroy_pcb(p4);
  destroy_counter(syscalls);
  destroy_exec_list(&exec);
}

Test(ks_exec_list, update_lowest_exec_priority_walks_the_list)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, true);
  t_pcb* p3 = create_pcb(EST_EXEC, 3);
  t_pcb* p7 = create_pcb(EST_EXEC, 7);
  transition_to_exec(p3, &exec);
  transition_to_exec(p7, &exec);

  exec.lowest_priority = 0;
  update_lowest_exec_priority(&exec);
  cr_assert_eq(exec.lowest_priority, 7);

  destroy_pcb(p3);
  destroy_pcb(p7);
  destroy_exec_list(&exec);
}

Test(ks_exec_list, wait_for_empty_returns_at_once_when_already_empty)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, false);
  wait_queue_exec_empty(&exec); /* must not hang */
  destroy_exec_list(&exec);
}

Test(ks_exec_list,
     wait_with_syscalls_returns_when_only_in_flight_syscalls_remain)
{
  t_execute_list exec;
  init_exec_list(&exec, 0, false);
  t_counter* syscalls = create_counter();
  t_pcb* a = create_pcb(EST_EXEC, 0);
  transition_to_exec(a, &exec);
  counter_increment(syscalls); /* the one process is inside a syscall */

  wait_queue_exec_empty_with_syscalls(&exec, syscalls); /* must not hang */

  transition_take_exec(a, &exec, syscalls);
  destroy_pcb(a);
  destroy_counter(syscalls);
  destroy_exec_list(&exec);
}

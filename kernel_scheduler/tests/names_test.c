#include <criterion/criterion.h>

#include "kernel_scheduler/app/kernel_scheduler.h"
#include "kernel_scheduler/connections/cpu.h"

Test(ks_names, state_names_follow_the_enum)
{
  cr_assert_str_eq(STATE_NAMES[PS_NEW], "NEW");
  cr_assert_str_eq(STATE_NAMES[PS_READY], "READY");
  cr_assert_str_eq(STATE_NAMES[PS_EXEC], "EXEC");
  cr_assert_str_eq(STATE_NAMES[PS_BLOCK], "BLOCK");
  cr_assert_str_eq(STATE_NAMES[PS_SUSP_READY], "SUSP. READY");
  cr_assert_str_eq(STATE_NAMES[PS_EXIT], "EXIT");
}

Test(ks_names, scheduling_algorithm_names_follow_the_enum)
{
  cr_assert_str_eq(SCHEDULING_ALGORITHMS[SA_FIFO], "FIFO");
  cr_assert_str_eq(SCHEDULING_ALGORITHMS[SA_RR], "RR");
  cr_assert_str_eq(SCHEDULING_ALGORITHMS[SA_MULTILEVEL_QUEUES], "MULTILEVEL");
}

Test(ks_names, syscall_names_follow_the_enum_order)
{
  cr_assert_str_eq(SYSCALL_NAMES[0], "MUTEX_CREATE");
  cr_assert_str_eq(SYSCALL_NAMES[5], "SLEEP");
  cr_assert_str_eq(SYSCALL_NAMES[9], "EXIT");
}

Test(ks_names, every_preemption_reason_has_a_description)
{
  cr_assert_str_eq(PREEMPTION_REASONS[PR_NO_PREEMPTION],
                   "no preemption occurred");
  cr_assert_str_eq(PREEMPTION_REASONS[PR_QUANTUM_END],
                   "preemption by quantum end");
  cr_assert_str_eq(PREEMPTION_REASONS[PR_SEGMENTATION_FAULT],
                   "segmentation fault");
}

#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

/** @brief Creates an empty list. */
void init_exec_list(t_execute_list* list, int quantum, bool preemption);

/** @brief Destroys the list (does not touch the PCBs it held). */
void destroy_exec_list(t_execute_list* list);

/** @brief Adds @p pcb, updating lowest_priority if preemption is enabled. */
void transition_to_exec(t_pcb* pcb, t_execute_list* exec);

/**
 * @brief Removes @p pcb, signaling anyone waiting on the exec/syscall-count
 *        gates and refreshing lowest_priority.
 */
void transition_take_exec(t_pcb* pcb, t_execute_list* exec,
                          t_counter* syscall_counter);

/** @brief Pops the head of the list, or NULL if empty. */
t_pcb* transition_take_exec_next(t_execute_list* exec);

/** @brief Recomputes lowest_priority from the PCBs currently in exec. */
void update_lowest_exec_priority(t_execute_list* exec);

// Whether the quantum that started at `start` (a millis() reading) is over.
bool quantum_ended(t_execute_list* exec, unsigned long start);

// Whether no process in exec has a lower priority than this one (i.e. it is
// (tied for) the worst-priority process currently running).
bool is_lowest_priority_in_exec(t_execute_list* exec, int priority);

// Blocks until EXEC is empty.
void wait_queue_exec_empty(t_execute_list* exec);
// Blocks until EXEC holds nothing but in-flight syscalls.
void wait_queue_exec_empty_with_syscalls(t_execute_list* exec,
                                         t_counter* syscall_counter);

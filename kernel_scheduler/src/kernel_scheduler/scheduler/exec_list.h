#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

void init_exec_list(t_execute_list* list, int quantum, bool preemption);
void destroy_exec_list(t_execute_list* list);

void transition_to_exec(t_pcb* pcb, t_execute_list* exec);
void transition_take_exec(t_pcb* pcb, t_execute_list* exec,
                          t_counter* syscall_counter);
t_pcb* transition_take_exec_next(t_execute_list* exec);

void update_lowest_exec_priority(t_execute_list* exec);

// Blocks until EXEC is empty.
void wait_queue_exec_empty(t_execute_list* exec);
// Blocks until EXEC holds nothing but in-flight syscalls.
void wait_queue_exec_empty_with_syscalls(t_execute_list* exec,
                                         t_counter* syscall_counter);

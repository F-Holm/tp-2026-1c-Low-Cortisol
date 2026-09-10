#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

void init_ready_queue(t_ready_queue* queue, int algorithm, t_list* cmn_algorithms);
void destroy_ready_queue(t_ready_queue* queue);

bool is_queue_ready_empty(t_ready_queue* ready);
bool is_queue_ready_blocked(t_ready_queue* ready);
void lock_queue_ready(t_ready_queue* ready);
void unlock_queue_ready(t_ready_queue* ready);
bool queue_ready_terminated(t_ready_queue* ready);
void terminate_queue_ready(t_ready_queue* ready);

// True unless the PCB's priority falls outside a multilevel queue's levels.
bool check_priority_valid(t_pcb* pcb, t_ready_queue* ready);

void transition_to_ready(t_pcb* pcb, t_ready_queue* ready);
void transition_take_ready(t_pcb* pcb, t_ready_queue* ready);
t_pcb* transition_take_ready_next(t_ready_queue* ready);
t_pcb* transition_take_ready_next_no_mutex(t_ready_queue* ready);

// Blocks until a schedulable PCB is available (or the queue is terminated,
// returning NULL). Respects the preempt-all gate.
t_pcb* transition_take_ready_blocking(t_ready_queue* ready);

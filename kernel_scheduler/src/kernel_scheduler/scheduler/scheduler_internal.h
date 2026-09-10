#pragma once

#include <stdbool.h>

#include "kernel_scheduler/scheduler/queue_types.h"

// Internals shared between queues.c (the state machine and the suspension
// worker threads) and compaction.c. Not part of the scheduler's public API --
// include the umbrella queues.h for that.

void increment_thread_counter(t_queues* queues);
void decrement_thread_counter(t_queues* queues);

// SUSP. READY -> READY for a process whose state_mutex the caller already
// holds. Returns false if it could not be resumed (no space, KM refusal).
bool transition_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues);

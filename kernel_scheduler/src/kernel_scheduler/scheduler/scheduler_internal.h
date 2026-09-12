#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/scheduler/queue_types.h"
#include "utils/log.h"

// Internals shared between queues.c (the state machine), suspension.c and
// compaction.c. Not part of the scheduler's public API -- include the umbrella
// queues.h for that.

void increment_thread_counter(t_queues* queues);
void decrement_thread_counter(t_queues* queues);

void log_transition_state(t_log* logger, uint32_t pid, int previous_state,
                          int state_new);
void log_invalid_state(t_log* logger, uint32_t pid, int state,
                       int expected_state, int next_state);

// If pcb->state == expected_state, logs the move, sets it to next_state and
// returns true; otherwise logs an invalid-transition error and returns false.
bool manage_state_pcb(t_log* logger, t_pcb* pcb, int expected_state,
                      int next_state);

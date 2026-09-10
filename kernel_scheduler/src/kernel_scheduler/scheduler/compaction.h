#pragma once

#include <stdbool.h>

#include "kernel_scheduler/scheduler/queue_types.h"

// The memory-compaction and resumption routines. Both run on detached threads
// and are serialised against each other by routine_active / routine_cond.

// Kicks off a resumption sweep of SUSP. READY (freed memory, a new Memory
// Stick, or the end of a compaction). No-op if a routine is already running.
void create_resumption_routine_thread(t_queues* queues);

// Runs a full compaction: quiesce every queue, ask Kernel Memory to compact,
// then release the queues and kick off a resumption sweep.
void routine_compaction(t_queues* queues);

bool is_compacting(t_queues* queues);
bool is_resuming(t_queues* queues);

// Reads Kernel Memory's verdict on whether a process being resumed fits.
// Assumes the caller holds km_socket->socket_mutex.
bool fits_process(t_queues* queues, t_pcb* process);

// Tells the routine threads to stop before their next tick (shutdown).
void terminate_routines(t_queues* queues);

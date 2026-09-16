#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

// The suspender and resumer worker threads. The suspender walks BLOCK and moves
// timed-out processes to SUSP. BLOCK (memory swapped out by Kernel Memory); the
// resumer walks SUSP. READY and brings processes back to READY when they fit.

/** @brief Creates and spawns the suspender and resumer threads. */
void start_threads_suspended(t_queues* queues, int suspension_timeout);

/** @brief Signals both threads to stop and joins them. */
void terminate_threads_suspended(t_queues* queues);

/** @brief Frees both threads' data (call after terminate_threads_suspended()).
 */
void destroy_threads_suspended(t_queues* queues);

/**
 * @brief Parks / wakes both worker threads (used around compaction and
 *        while a single suspend/resume is in flight).
 */
void lock_threads_suspended(t_queues* queues);
void unlock_threads_suspended(t_queues* queues);

/**
 * @brief State transitions owned by the suspension subsystem: BLOCK ->
 *        SUSP. BLOCK, SUSP. BLOCK -> SUSP. READY, and SUSP. READY -> READY
 *        (only if there's enough space; returns whether it succeeded).
 * @note The caller must hold pcb->state_mutex.
 */
void transition_block_susp_block_no_mutex(t_pcb* pcb, t_queues* queues);
void transition_susp_block_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues);
bool transition_susp_ready_no_mutex(t_pcb* pcb, t_queues* queues);

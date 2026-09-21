#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "utils/collections/list.h"

/**
 * @brief Creates an empty ready queue. Pass @p multilevel_algorithms (one
 *        subqueue per level) when @p algorithm is SA_MULTILEVEL_QUEUES, NULL
 *        otherwise.
 */
void init_ready_queue(t_ready_queue* queue, int algorithm,
                      t_list* multilevel_algorithms);

/** @brief Destroys every subqueue and the queue itself (does not touch the
 *         PCBs it held). */
void destroy_ready_queue(t_ready_queue* queue);

/** @brief Whether every subqueue currently holds no PCB. */
bool is_queue_ready_empty(t_ready_queue* ready);

/** @brief Whether the queue is currently blocked (see lock_queue_ready()). */
bool is_queue_ready_blocked(t_ready_queue* ready);

/** @brief Gates transition_take_ready_blocking() from handing out PCBs,
 *         used to quiesce the ready queue (e.g. before compaction). */
void lock_queue_ready(t_ready_queue* ready);

/** @brief Lifts the lock_queue_ready() gate and wakes blocked takers. */
void unlock_queue_ready(t_ready_queue* ready);

/** @brief Whether terminate_queue_ready() was called. */
bool queue_ready_terminated(t_ready_queue* ready);

/** @brief Marks the queue terminated and wakes every blocked taker (they
 *         then return NULL); used on shutdown. */
void terminate_queue_ready(t_ready_queue* ready);

// True unless the PCB's priority falls outside a multilevel queue's levels.
bool check_priority_valid(t_pcb* pcb, t_ready_queue* ready);

// The scheduling algorithm in effect at this priority level (of the single
// queue, if `ready` is not a multilevel queue).
int get_algorithm_ready_queue(t_ready_queue* ready, int priority);

/** @brief Adds @p pcb to its level's subqueue. */
void transition_to_ready(t_pcb* pcb, t_ready_queue* ready);

/** @brief Removes a specific @p pcb from its level's subqueue. */
void transition_take_ready(t_pcb* pcb, t_ready_queue* ready);

/** @brief Pops the head of the highest-priority non-empty subqueue, or NULL
 *         if every subqueue is empty. */
t_pcb* transition_take_ready_next(t_ready_queue* ready);
t_pcb* transition_take_ready_next_no_mutex(t_ready_queue* ready);

// Blocks until a schedulable PCB is available (or the queue is terminated,
// returning NULL). Respects the preempt-all gate.
t_pcb* transition_take_ready_blocking(t_ready_queue* ready);

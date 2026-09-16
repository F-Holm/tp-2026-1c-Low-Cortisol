#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

// BLOCK / SUSP. BLOCK / SUSP. READY as data structures: adding a PCB, taking a
// specific PCB out, and popping the head. SUSP. BLOCK and SUSP. READY are kept
// sorted by priority; BLOCK is FIFO and stamps the blocked-at timestamp.

/** @brief Creates an empty list. */
void init_blocking_list(t_blocking_list* list);

/** @brief Destroys the list (does not touch the PCBs it held). */
void destroy_blocking_list(t_blocking_list* list);

/** @brief Whether the list currently holds no PCB. */
bool blocking_list_is_empty(t_blocking_list* list);

/**
 * @brief Inserts @p pcb into the list (transition_to_block also stamps its
 *        blocked-at timestamp and signals the suspender thread).
 */
void transition_to_block(t_pcb* pcb, t_blocking_list* block);
void transition_to_susp_block(t_pcb* pcb, t_blocking_list* susp_block);
/** @brief Inserts @p pcb into SUSP. READY, signaling the resumer thread if
 *         the list was empty. */
void transition_to_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready);

/**
 * @brief Removes a specific @p pcb from the list (transition_take_block also
 *        clears its blocked-at timestamp).
 */
void transition_take_block(t_pcb* pcb, t_blocking_list* block);
void transition_take_susp_block(t_pcb* pcb, t_blocking_list* susp_block);
void transition_take_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready);

/**
 * @brief Pops the head of the list, or NULL if empty (transition_take_block_
 *        next also clears the popped PCB's blocked-at timestamp).
 */
t_pcb* transition_take_block_next(t_blocking_list* block);
t_pcb* transition_take_susp_block_next(t_blocking_list* susp_block);
t_pcb* transition_take_susp_ready_next(t_blocking_list* susp_ready);

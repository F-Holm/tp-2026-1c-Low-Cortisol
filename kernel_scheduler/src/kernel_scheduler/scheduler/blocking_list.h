#pragma once

#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"

// BLOCK / SUSP. BLOCK / SUSP. READY as data structures: adding a PCB, taking a
// specific PCB out, and popping the head. SUSP. BLOCK and SUSP. READY are kept
// sorted by priority; BLOCK is FIFO and stamps the blocked-at timestamp.

void init_blocking_list(t_blocking_list* list);
void destroy_blocking_list(t_blocking_list* list);
bool blocking_list_is_empty(t_blocking_list* list);

void transition_to_block(t_pcb* pcb, t_blocking_list* block);
void transition_to_susp_block(t_pcb* pcb, t_blocking_list* susp_block);
void transition_to_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready);

void transition_take_block(t_pcb* pcb, t_blocking_list* block);
t_pcb* transition_take_block_next(t_blocking_list* block);
void transition_take_susp_block(t_pcb* pcb, t_blocking_list* susp_block);
t_pcb* transition_take_susp_block_next(t_blocking_list* susp_block);
void transition_take_susp_ready(t_pcb* pcb, t_blocking_list* susp_ready);
t_pcb* transition_take_susp_ready_next(t_blocking_list* susp_ready);

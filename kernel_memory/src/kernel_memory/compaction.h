#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

/**
 * @brief Slides every segment down to close the gaps, then replaces the hole
 *        table with a single trailing hole. Blocks for
 *        `main_memory->compaction_delay` ms and tells the scheduler when
 *        it's done.
 */
bool compact_memory(int socket_scheduler, t_main_memory* main_memory);

/** @brief Reassigns each segment's base so they sit contiguously from 0. */
void compact_segments(t_list* segments);

/** @brief Address right after the last segment. */
int compute_last_segment_end(t_list* segments);

/** @brief Builds a single hole covering the rest of memory after compaction. */
t_list* compact_holes(int memory_total, int base_final_segment);

/**
 * @brief Tells the scheduler a compaction is needed and waits for it to
 *        grant it (OP_CAN_COMPACT) before returning.
 */
void notify_compaction(int socket_scheduler);

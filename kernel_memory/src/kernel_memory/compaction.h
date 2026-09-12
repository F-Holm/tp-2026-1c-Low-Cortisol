#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

// Slides every segment down to close the gaps, then replaces the hole table
// with a single trailing hole. Blocks for `main_memory->compaction_delay` ms
// and tells the scheduler when it's done.
bool compact_memory(int socket_scheduler, t_main_memory* main_memory);

void compact_segments(t_list* segments);
int compute_last_segment_end(t_list* segments);
t_list* compact_holes(int memory_total, int base_final_segment);

// Tells the scheduler a compaction is needed and waits for it to grant it
// (OP_CAN_COMPACT) before returning.
void notify_compaction(int socket_scheduler);

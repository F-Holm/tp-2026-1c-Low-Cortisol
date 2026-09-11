#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

// Removes and returns the segment matching id+pid, or NULL (and logs) if
// there is none.
t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger);

// The MEM_FREE entry point: removes the segment and merges the space it frees
// with any adjacent hole(s).
void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger);

bool hole_before_segment(int base_segment, int final_segment, t_list* holes);
bool hole_after_segment(int base_segment, int final_segment, t_list* holes);

t_segment* find_segment(t_main_memory* main_memory, uint32_t pid,
                        uint32_t segment_number);

t_list* filter_process_segments(int pid, t_main_memory* main_memory,
                                t_log* logger);
void add_segments_to_packet(t_list* segments, t_packet* process_segment_table);

int compute_process_size(t_process* process, t_main_memory* main_memory);

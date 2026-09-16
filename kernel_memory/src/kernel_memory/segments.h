#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/**
 * @brief Removes and returns the segment matching id+pid.
 * @return NULL (and logs) if there is none.
 */
t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger);

/**
 * @brief The MEM_FREE entry point: removes the segment and merges the space
 *        it frees with any adjacent hole(s).
 */
void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger);

/** @brief Whether a hole ends exactly at @p base_segment. */
bool hole_before_segment(int base_segment, int final_segment, t_list* holes);

/** @brief Whether a hole starts exactly at @p final_segment. */
bool hole_after_segment(int base_segment, int final_segment, t_list* holes);

/** @brief Finds a process's Nth segment (0-indexed, in creation order). */
t_segment* find_segment(t_main_memory* main_memory, uint32_t pid,
                        uint32_t segment_number);

/**
 * @brief Collects @p pid's segments into a new list of shared pointers.
 * @note Free the returned list without touching the elements.
 */
t_list* filter_process_segments(int pid, t_main_memory* main_memory,
                                t_log* logger);

/** @brief Appends each segment's raw data to the packet. */
void add_segments_to_packet(t_list* segments, t_packet* process_segment_table);

/** @brief Sums the size of @p process's segments currently in memory. */
int compute_process_size(t_process* process, t_main_memory* main_memory);

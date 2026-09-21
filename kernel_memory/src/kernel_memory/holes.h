#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/mutex.h"

/** @brief Sums the size of every hole. */
int compute_free_space(t_list* holes, mtx_t* holes_mutex, t_log* logger);

/**
 * @brief Adds `memory_total` bytes at the end of the address space (a Memory
 *        Stick connecting), extending the trailing hole or creating one.
 */
t_main_memory* add_total_memory(t_main_memory* main_memory, int memory_total);

/**
 * @brief Picks a hole per the configured allocation strategy and shrinks it
 *        by `size`.
 * @return A hole with size == -1 if none fit.
 */
t_hole select_hole(uint32_t size, t_log* logger, t_main_memory* memory);

/** @brief Appends a new segment covering @p chosen_hole to the segment list. */
void update_segment_list(t_main_memory* main_memory, t_hole chosen_hole,
                         int size, uint32_t pid, uint32_t id);

/**
 * @brief The MEM_ALLOC entry point: checks the segment-size and free-space
 *        limits, selects a hole (compacting first if none fits) and replies
 *        to the scheduler.
 */
void create_segment(uint32_t id, uint32_t pid, int size,
                    t_main_memory* main_memory, t_socket* socket_scheduler,
                    t_log* logger);

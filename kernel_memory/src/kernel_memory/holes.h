#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

int compute_free_space(t_list* holes, pthread_mutex_t* holes_mutex,
                       t_log* logger);

// Adds `memory_total` bytes at the end of the address space (a Memory Stick
// connecting), extending the trailing hole or creating one.
t_main_memory* add_total_memory(t_main_memory* main_memory, int memory_total);

// Picks a hole per the configured allocation strategy and shrinks it by
// `size`. A hole with size == -1 means none fit.
t_hole select_hole(uint32_t size, t_log* logger, t_main_memory* memory);

void update_segment_list(t_main_memory* main_memory, t_hole chosen_hole,
                         int size, uint32_t pid, uint32_t id);

// The MEM_ALLOC entry point: checks the segment-size and free-space limits,
// selects a hole (compacting first if none fits) and replies to the
// scheduler.
void create_segment(uint32_t id, uint32_t pid, int size,
                    t_main_memory* main_memory, int socket_scheduler,
                    t_log* logger);

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"

/** @brief Allocates and initializes the module's top-level
 * t_kernel_memory_data. */
t_kernel_memory_data* init_kernel_memory_data(
    t_socket* socket_kernel_memory, char* scripts_basepath,
    int instruction_delay, int compaction_delay, int segment_max_size,
    t_allocation_strategy allocation_strategy, t_log* logger);

/** @brief Allocates and initializes a scheduler connection's t_scheduler_data.
 */
t_scheduler_data* init_scheduler_data(
    t_socket* socket_kernel_memory, t_socket* socket_scheduler,
    t_list* processes, char* scripts_basepath, mtx_t* processes_mutex,
    t_main_memory* main_memory, t_list* connected_sticks, mtx_t* sticks_mutex,
    _Atomic(t_swap_data*)* swap_data, t_log* logger, int* active_threads,
    mtx_t* active_threads_mutex, cnd_t* active_threads_cond);

/** @brief Allocates and initializes a CPU connection's t_cpu_data. */
t_cpu_data* init_cpu_data(t_socket* socket_cpu, t_list* processes,
                          mtx_t* processes_mutex, int instruction_delay,
                          t_main_memory* main_memory, t_log* logger,
                          int* active_threads, mtx_t* active_threads_mutex,
                          cnd_t* active_threads_cond,
                          t_socket* socket_scheduler);

/** @brief Allocates a t_stick_data with size/port left unset (-1). */
t_stick_data* init_stick_data(t_socket* socket_stick, t_log* logger,
                              t_socket* socket_scheduler);

/**
 * @brief Allocates a t_swap_data and reads the swap's block/size info off
 *        the socket (OP_INFO_SWAP handshake).
 * @return NULL if the peer didn't send the expected handshake.
 */
t_swap_data* init_swap_data(t_socket* socket_swap, t_log* logger);

/**
 * @brief Allocates a t_process and loads its instructions from
 *        `scripts_basepath`/`relative_path`.
 * @return NULL if the instructions file could not be opened.
 */
t_process* init_process(uint32_t pid, char* relative_path,
                        char* scripts_basepath, t_log* logger);

/**
 * @brief Fills `stick_data->ip_memory_stick` from the socket's peer address.
 * @return false if the peer address could not be resolved.
 */
bool resolve_stick_ip(t_stick_data* stick_data, t_socket* client_socket);

/**
 * @brief Allocates an empty t_main_memory.
 * @param total_size  Used as the max segment size; the memory's own
 *                     total_size starts at 0 and grows as Memory Sticks
 *                     connect.
 */
t_main_memory* init_main_memory(int total_size,
                                t_allocation_strategy allocation_strategy,
                                int compaction_delay);

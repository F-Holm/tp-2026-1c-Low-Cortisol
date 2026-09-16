#pragma once

#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

/** @brief Frees a fully torn-down t_kernel_memory_data, waiting for every
 *         listener thread to exit first. */
void free_kernel_memory_data(t_kernel_memory_data* kernel_data);

/** @brief Closes the scheduler connection's sockets and frees its data. */
void free_scheduler_data(t_scheduler_data* scheduler_data);

/** @brief Closes the CPU's socket and frees its t_cpu_data. */
void free_cpu_data(t_cpu_data* cpu_data);

/** @brief Closes the CPU's socket connection, if still open. */
void close_cpu(t_cpu_data* cpu);

/** @brief Closes the stick's socket and frees its t_stick_data. NULL-safe. */
void free_stick_data(t_stick_data* stick_data);

/** @brief Frees the swap connection data and its block list. NULL-safe. */
void free_swap_data(t_swap_data* swap_data);

/** @brief Frees a process's instructions and segment list, then the process. */
void free_process(t_process* process);

/** @brief Frees main memory's segment/hole lists and its mutex. NULL-safe. */
void free_main_memory(t_main_memory* memory);

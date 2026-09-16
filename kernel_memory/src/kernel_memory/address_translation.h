#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

/**
 * @brief Resolves a logical address to a physical one via the process's
 *        segment table.
 * @return -1 if the address falls in a segment the process doesn't have.
 */
int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger);

/**
 * @brief Finds the connected stick holding @p physical_address.
 * @param stick_offset  Out-param: offset within the stick.
 * @return Index into @p connected_sticks, or -1 if none covers it.
 */
int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset);

/**
 * @brief Reads @p size bytes starting at @p physical_address, possibly
 *        spanning multiple Memory Sticks.
 * @return Newly allocated buffer (caller frees), or NULL on failure.
 */
char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler);

/**
 * @brief Writes bytes to physical memory, possibly spanning multiple Memory
 *        Sticks.
 * @return false if a stick write failed.
 */
bool write_to_sticks(int pid, int physical_address, int bytes_to_read,
                     char* write_string, t_list* connected_sticks,
                     pthread_mutex_t* socket_list_mutex, t_log* logger,
                     int socket_scheduler);

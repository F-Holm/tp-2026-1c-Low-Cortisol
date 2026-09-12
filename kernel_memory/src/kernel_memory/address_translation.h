#pragma once

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

// Logical -> physical address, resolved through the process's segment table.
// -1 if the logical address falls in a segment the process doesn't have.
int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger);

// Index into `connected_sticks` of the stick holding `physical_address`, and
// the offset within it (via `stick_offset`). -1 if no stick covers it.
int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset);

// Reads/writes possibly spanning multiple Memory Sticks.
char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler);
bool write_to_sticks(int pid, int physical_address, int bytes_to_read,
                     char* write_string, t_list* connected_sticks,
                     pthread_mutex_t* socket_list_mutex, t_log* logger,
                     int socket_scheduler);

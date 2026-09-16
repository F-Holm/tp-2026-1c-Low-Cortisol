#pragma once

#include <stdio.h>

#include "cpu/cpu.h"

#define INVALID_ADDRESS UINT32_MAX

/**
 * @brief Translates @p logical_address (of @p size bytes) for @p pid into a
 *        physical address using @p context's segment table.
 * @return The physical address, INVALID_ADDRESS after notifying a segfault,
 *         or INVALID_ADDRESS - 1 on error (segment missing or notify failed).
 */
uint32_t mmu(t_cpu* cpu, t_context* context, uint32_t logical_address,
             uint32_t size, uint32_t pid);

/** @brief Finds the segment with @p segment_number in @p segment_table. @return
 * NULL if not found. */
t_segment* find_segment_by_id(t_list* segment_table, uint32_t segment_number);

/** @brief Notifies the Kernel Scheduler of a segmentation fault for @p pid.
 * @return false on failure. */
bool notify_seg_fault(t_cpu* cpu, uint32_t pid);

/** @brief Finds the Memory Stick that covers @p physical_address. @return NULL
 * if none does. */
t_memory_stick_info* find_stick(t_cpu* cpu, uint32_t physical_address);

/**
 * @brief Reads @p size bytes starting at @p physical_address, spanning
 *        Memory Sticks as needed.
 * @return The data, or NULL on failure. Caller frees it.
 */
void* read_memory(t_cpu* cpu, uint32_t physical_address, uint32_t size);

/** @brief Sends a read request to @p stick. @return false on failure (notifies
 * BSOD). */
bool request_read(t_cpu* cpu, t_memory_stick_info* stick,
                  uint32_t address_in_stick, uint32_t bytes_to_read);

/**
 * @brief Receives the data from a prior request_read().
 * @return The read bytes, or NULL on failure (notifies BSOD). Caller frees it.
 */
char* receive_read_response(t_cpu* cpu, t_memory_stick_info* stick);

/**
 * @brief Writes @p size bytes of @p data_to_write starting at
 *        @p physical_address, spanning Memory Sticks as needed.
 * @return false on failure.
 */
bool write_memory(t_cpu* cpu, uint32_t physical_address, void* data_to_write,
                  uint32_t size);

/** @brief Sends a write request to @p stick. @return false on failure (notifies
 * BSOD). */
bool request_write(t_cpu* cpu, t_memory_stick_info* stick,
                   uint32_t address_in_stick, void* data,
                   uint32_t bytes_to_write);

/** @brief Receives the ack from a prior request_write(). @return false on
 * failure or disconnect. */
bool receive_write_response(t_cpu* cpu, t_memory_stick_info* stick);

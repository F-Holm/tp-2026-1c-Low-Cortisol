#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cpu/cpu.h"
#include "utils/collections/list.h"
#include "utils/sockets.h"

/** @brief Connects to Kernel Memory and performs the handshake. @return false
 * on failure. */
bool connect_to_kernel_memory(t_cpu* cpu);

/** @brief Connects to the Kernel Scheduler and performs the handshake. @return
 * false on failure. */
bool connect_to_kernel_scheduler(t_cpu* cpu);

/**
 * @brief Receives a newly connected Memory Stick's info from Kernel Memory,
 *        connects to it and adds it to @p cpu's memory_sticks list.
 * @return false on failure.
 */
bool connect_memory_stick(t_cpu* cpu);

/** @brief Sums the sizes of @p sticks to get the offset for the next one. */
uint32_t compute_offset(t_list* sticks);

/**
 * @brief Handshakes with a Memory Stick over @p new_socket.
 * @return false on failure (also destroys @p new_socket).
 */
bool handshake_memory_stick(t_cpu* cpu, t_socket* new_socket);

/** @brief Notifies Kernel Memory that a Memory Stick disconnected. */
void notify_bsod(t_cpu* cpu);

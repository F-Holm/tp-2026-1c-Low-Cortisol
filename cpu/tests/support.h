#pragma once

#include "cpu/cpu.h"
#include "cpu/registers.h"

/**
 * @file
 * @brief Shared helpers for the cpu module test suites.
 */

/** @brief A console-only logger that stays silent below ERROR. */
t_log* cpu_quiet_logger(void);

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side fd.
 * @return The client-side fd. Both fds must be closed by the caller.
 */
int cpu_connected_pair(int* server_out);

/** @brief A zeroed execution context with an empty segment table. */
t_context* cpu_make_context(void);

/** @brief Frees a context built with `cpu_make_context()` and its segments. */
void cpu_destroy_context(t_context* context);

/** @brief A heap `t_segment` with the given fields (pid is left at 0). */
t_segment* cpu_make_segment(uint32_t id, int base, int size);

/** @brief A heap `t_memory_stick_info` (socket left at 0). */
t_memory_stick_info* cpu_make_stick(uint32_t offset, uint32_t size);

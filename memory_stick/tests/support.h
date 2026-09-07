#pragma once

#include "memory_stick/memory_stick.h"

/**
 * @file
 * @brief Shared helpers for the memory_stick module test suites.
 */

/** @brief A console-only logger that stays silent below ERROR. */
t_log* ms_quiet_logger(void);

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side fd.
 * @return The client-side fd. Both fds must be closed by the caller.
 */
int ms_connected_pair(int* server_out);

/**
 * @brief A `t_ms` with a zeroed @p memory_size byte memory, an initialised
 *        mutex, a quiet logger and no delay. Free it with `ms_destroy()`.
 */
t_ms* ms_make(int memory_size);

/** @brief Frees a `t_ms` built with `ms_make()`. */
void ms_destroy(t_ms* ms);

/**
 * @brief Writes a throwaway memory_stick config file under /tmp.
 * @return Its path (heap-allocated); the caller unlinks the file and frees
 * this.
 */
char* ms_write_temp_config(void);

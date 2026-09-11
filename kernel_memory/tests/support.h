#pragma once

#include "kernel_memory/structs.h"

/**
 * @file
 * @brief Shared helpers for the kernel_memory module test suites.
 */

/** @brief A console-only logger that stays silent below ERROR. */
t_log* km_quiet_logger(void);

/** @brief A heap `t_hole` with the given base and size. */
t_hole* km_make_hole(int base, int size);

/** @brief A heap `t_segment` (registers-cpu struct) for @p pid. */
t_segment* km_make_segment(uint32_t id, uint32_t pid, int base, int size);

/** @brief A `t_stick_data` carrying just a size. */
t_stick_data* km_make_stick(int size);

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side fd.
 * @return The client-side fd. Both fds must be closed by the caller.
 */
int km_connected_pair(int* server_out);

/**
 * @brief Creates a listening socket on a kernel-assigned ephemeral port.
 * @param port_out Buffer that receives the decimal port as a string.
 * @param port_len Size of @p port_out.
 * @return The listening fd, to be passed to `accept()` and closed by the
 * caller.
 */
int km_listen_ephemeral(char* port_out, int port_len);

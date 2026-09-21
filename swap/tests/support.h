#pragma once

#include <stdio.h>

#include "swap/swap.h"

/**
 * @file
 * @brief Shared helpers for the swap module test suites.
 */

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side socket.
 * @return The client-side socket. Both sockets must be destroyed by the
 * caller.
 */
t_socket* swap_connected_pair(t_socket** server_out);

/**
 * @brief Creates a listening socket on a kernel-assigned ephemeral port.
 * @param port_out Buffer that receives the decimal port as a string.
 * @param port_len Size of @p port_out.
 * @return The listening socket, to be passed to `socket_accept()` and
 * destroyed by the caller.
 */
t_socket* swap_listen_ephemeral(char* port_out, int port_len);

/**
 * @brief Writes a throwaway swap config file under /tmp.
 * @param swap_file_path Value for the `SWAP_FILE_PATH` key.
 * @return Its path (heap-allocated); the caller unlinks the file and frees
 * this.
 */
char* swap_write_temp_config(const char* swap_file_path);

/** @brief A console-only logger that stays silent below ERROR. */
t_log* swap_quiet_logger(void);

/** @brief A temporary file truncated to @p size bytes of zeros. */
FILE* swap_sized_tmpfile(int size);

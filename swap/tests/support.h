#pragma once

#include <stdio.h>

#include "swap/swap.h"

/**
 * @file
 * @brief Shared helpers for the swap module test suites.
 */

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side fd.
 * @return The client-side fd. Both fds must be closed by the caller.
 */
int swap_connected_pair(int* server_out);

/**
 * @brief Creates a listening socket on a kernel-assigned ephemeral port.
 * @param port_out Buffer that receives the decimal port as a string.
 * @param port_len Size of @p port_out.
 * @return The listening fd, to be passed to `accept()` and closed by the
 * caller.
 */
int swap_listen_ephemeral(char* port_out, int port_len);

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

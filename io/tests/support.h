#pragma once

#include "utils/log.h"
#include "utils/sockets.h"

/**
 * @file
 * @brief Shared helpers for the io module test suites.
 */

/**
 * @brief Opens a loopback TCP connection on a kernel-assigned port.
 * @param server_out Set to the accepted server-side socket.
 * @return The client-side socket. Both sockets must be destroyed by the
 * caller.
 */
t_socket* io_connected_pair(t_socket** server_out);

/**
 * @brief Creates a listening socket on a kernel-assigned ephemeral port.
 * @param port_out Buffer that receives the decimal port as a string.
 * @param port_len Size of @p port_out.
 * @return The listening socket, to be passed to `socket_accept()` and
 * destroyed by the caller.
 */
t_socket* io_listen_ephemeral(char* port_out, int port_len);

/**
 * @brief Writes a throwaway io config file under /tmp.
 * @return Its path (heap-allocated); the caller unlinks the file and frees
 * this.
 *
 * The file sets `LOG_LEVEL`, `KERNEL_SCHEDULER_IP` and `KERNEL_SCHEDULER_PORT`.
 */
char* io_write_temp_config(void);

/** @brief A console-only logger that stays silent below ERROR. */
t_log* io_quiet_logger(void);

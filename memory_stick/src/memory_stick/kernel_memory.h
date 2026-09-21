#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils/log.h"
#include "utils/sockets.h"

/**
 * @brief Performs the handshake with Kernel Memory.
 * @return false if the handshake failed.
 */
bool handshake_km(t_socket* socket_km, t_log* logger);

/**
 * @brief Reports this stick's memory size to Kernel Memory.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_size(t_socket* socket_km, char* size, t_log* logger);

/**
 * @brief Connects to Kernel Memory, handshakes and reports the stick's size.
 * @return The connected socket, or NULL on failure.
 */
t_socket* connect_to_kernel_memory(char* ip, char* port, char* size,
                                   t_log* logger);

/**
 * @brief Opens the TCP connection to Kernel Memory.
 * @return The socket, or NULL on failure.
 */
t_socket* connect_km(char* ip, char* port, t_log* logger);

/**
 * @brief Sends the CPU server's listen port to Kernel Memory.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_cpu_server_port_to_km(t_socket* socket, uint16_t port);

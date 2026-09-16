#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils/log.h"

/**
 * @brief Performs the handshake with Kernel Memory.
 * @return false if the handshake failed.
 */
bool handshake_km(int socket_km, t_log* logger);

/**
 * @brief Reports this stick's memory size to Kernel Memory.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_size(int socket_km, char* size, t_log* logger);

/**
 * @brief Connects to Kernel Memory, handshakes and reports the stick's size.
 * @return The connected socket fd, or -1 on failure.
 */
int connect_to_kernel_memory(char* ip, char* port, char* size, t_log* logger);

/**
 * @brief Opens the TCP connection to Kernel Memory.
 * @return The socket fd, or -1 on failure.
 */
int connect_km(char* ip, char* port, t_log* logger);

/**
 * @brief Sends the CPU server's listen port to Kernel Memory.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_cpu_server_port_to_km(int socket, uint16_t port);

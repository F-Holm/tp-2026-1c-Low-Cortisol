#pragma once

#include <stdbool.h>

#include "kernel_memory/structs.h"
#include "utils/sockets.h"

/**
 * @brief Reads the peer's handshake and routes it to the appropriate
 *        per-module init (Kernel Scheduler, CPU, Memory Stick, or Swap).
 * @return false on a handshake failure or unknown module id.
 */
bool handshake(t_kernel_memory_data* kernel_data, t_socket* client_socket);

/** @brief Accepts one client connection and runs its handshake(). */
bool accept_client(void* ptr);

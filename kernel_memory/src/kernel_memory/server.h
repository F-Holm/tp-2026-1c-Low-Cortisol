#pragma once

#include "kernel_memory/configurator.h"
#include "kernel_memory/cpu_listener.h"
#include "kernel_memory/error.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/protocol.h"
#include "kernel_memory/scheduler_listener.h"
#include "kernel_memory/structs.h"
#include "utils/msg.h"

/**
 * @brief Reads the peer's handshake and routes it to the appropriate
 *        per-module init (Kernel Scheduler, CPU, Memory Stick, or Swap).
 * @return false on a handshake failure or unknown module id.
 */
bool handshake(t_kernel_memory_data* kernel_data, int client_socket);

/** @brief Accepts one client connection and runs its handshake(). */
bool accept_client(void* ptr);

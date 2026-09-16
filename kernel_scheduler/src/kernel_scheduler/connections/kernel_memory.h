#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"
#include "utils/log.h"

typedef struct
{
  t_log* logger;
  t_kernel_memory_socket* km_socket;
  atomic_bool close;
  pthread_t thread;
} t_connection_check_thread;

/**
 * @brief Connects and handshakes with Kernel Memory.
 * @return The connected socket fd, or -1 on failure.
 */
int start_connection_kernel_memory(char* ip, char* port, t_log* logger);

/**
 * @brief Sends OP_END_PROCESS for @p pid.
 * @return false on send failure (also triggers a scheduler shutdown).
 */
bool notify_terminate_process(t_kernel_memory_socket* km_socket, uint32_t pid);

/** @brief Spawns the background thread that periodically pings Kernel
 *         Memory to detect a dropped connection. */
t_connection_check_thread* start_thread_check_connection_kernel_memory(
    t_log* logger, t_kernel_memory_socket* km_socket);

/** @brief Stops and joins the connection-check thread, then frees @p data. */
void destroy_thread_check_connection_kernel_memory(
    t_connection_check_thread* data);

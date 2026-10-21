#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/shutdown.h"
#include "utils/log.h"
#include "utils/sockets.h"
#include "utils/watchdog.h"

/**
 * @brief Connects and handshakes with Kernel Memory.
 * @return The connected socket fd, or -1 on failure.
 */
t_socket* start_connection_kernel_memory(char* ip, char* port, t_log* logger);

/**
 * @brief Sends OP_END_PROCESS for @p pid.
 * @return false on send failure (also triggers a scheduler shutdown).
 */
bool notify_terminate_process(t_socket* km_socket, t_shutdown_ctx* shutdown,
                              uint32_t pid);

/** @brief Spawns the background thread that periodically pings Kernel
 *         Memory to detect a dropped connection (see utils/watchdog.h). On a
 *         failed ping, shuts the scheduler down through @p shutdown. */
t_watchdog* start_thread_check_connection_kernel_memory(
    t_log* logger, t_socket* km_socket, t_shutdown_ctx* shutdown);

/** @brief Stops and joins the connection-check thread, then frees @p watchdog.
 */
void destroy_thread_check_connection_kernel_memory(t_watchdog* watchdog);

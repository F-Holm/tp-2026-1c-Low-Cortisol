#pragma once

#include "utils/log.h"
#include "utils/sockets.h"

typedef enum
{
  SR_NO_PROCESSES,
  SR_CORRUPTED_MEMORY,
  SR_KERNEL_MEMORY_CONNECTION_FAILURE,
  SR_UNKNOWN_CAUSE,
  SR_KERNEL_MEMORY_SEND_ERROR
} t_shutdown_reason;

extern const char* const SHUTDOWN_REASONS[4];

/** @brief Everything close_kernel_scheduler() needs. Always passed around by
 *         pointer -- never kept as module-level state. */
typedef struct
{
  t_socket* server_socket;
  t_log* logger;
  t_socket* km_socket;
} t_shutdown_ctx;

/** @brief Creates the shutdown context. Must happen once, before any thread
 *         can reach close_kernel_scheduler() with it. Free it with
 *         destroy_shutdown(). */
t_shutdown_ctx* init_shutdown(t_socket* server_socket, t_log* logger,
                              t_socket* km_socket);

/** @brief Frees @p ctx. */
void destroy_shutdown(t_shutdown_ctx* ctx);

/**
 * @brief Shuts the scheduler down for @p reason_shutdown: notifies Kernel
 *        Memory if applicable, logs it and closes the server socket.
 * @note Idempotent: only the first call has effect.
 */
void close_kernel_scheduler(t_shutdown_ctx* ctx, int reason_shutdown);

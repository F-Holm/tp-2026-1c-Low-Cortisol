#pragma once

#include "utils/log.h"

typedef enum
{
  SR_NO_PROCESSES,
  SR_CORRUPTED_MEMORY,
  SR_KERNEL_MEMORY_CONNECTION_FAILURE,
  SR_UNKNOWN_CAUSE,
  SR_KERNEL_MEMORY_SEND_ERROR
} t_shutdown_reason;

extern const char* const SHUTDOWN_REASONS[4];

// Idempotent: only the first caller runs the shutdown sequence (notify Kernel
// Memory, resolve the final reason, log it and shut the server socket down),
// every later call returns immediately.
void close_kernel_scheduler(int server_socket, t_log* logger,
                            int reason_shutdown, int km_socket);

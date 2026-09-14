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

// Must be called once, before any thread can reach close_kernel_scheduler.
void init_shutdown(int server_socket, t_log* logger, int km_socket);
void close_kernel_scheduler(int reason_shutdown);

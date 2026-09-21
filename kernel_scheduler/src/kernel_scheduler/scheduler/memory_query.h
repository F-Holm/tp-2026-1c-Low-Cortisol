#pragma once

#include <stdint.h>

#include "kernel_scheduler/scheduler/queue_types.h"

// Request/response exchanges with Kernel Memory about memory sizes. The
// `_no_mutex` variants assume the caller already holds km_socket->socket_mutex;
// the plain ones take it. A reply of OP_NEW_MEMORY_STICK is handled inline by
// kicking off a resumption routine and retrying the read.

/**
 * @brief Asks Kernel Memory for the current free space.
 * @return The free space, or -1 on a communication error.
 */
int space_available_no_mutex(t_queues* queues, uint32_t pid);
int space_available(t_queues* queues, uint32_t pid);

/**
 * @brief Asks Kernel Memory for @p pid's size in memory.
 * @return The size, or -1 on a communication error.
 */
int process_size_no_mutex(t_queues* queues, uint32_t pid);
int process_size(t_queues* queues, uint32_t pid);

/** @brief Same as process_size(), without the log_trace() on success. */
int process_size_no_logger(t_queues* queues, uint32_t pid);

#pragma once

#include <stdint.h>

#include "kernel_scheduler/scheduler/queue_types.h"

// Request/response exchanges with Kernel Memory about memory sizes. The
// `_no_mutex` variants assume the caller already holds km_socket->socket_mutex;
// the plain ones take it. A reply of OP_NEW_MEMORY_STICK is handled inline by
// kicking off a resumption routine and retrying the read.

int space_available_no_mutex(t_queues* queues, uint32_t pid);
int space_available(t_queues* queues, uint32_t pid);
int process_size_no_mutex(t_queues* queues, uint32_t pid);
int process_size(t_queues* queues, uint32_t pid);
int process_size_no_logger(t_queues* queues, uint32_t pid);

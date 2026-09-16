#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/scheduler/queues.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

/**
 * @brief Handles a MEM_ALLOC syscall: checks free space, asks Kernel Memory
 *        to create the segment and waits for its reply.
 * @return false if there isn't enough space or the segment is rejected.
 */
bool allocate_memory(t_syscall_memory* mem_alloc, t_queues* queues);

/**
 * @brief Handles a MEM_FREE syscall: asks Kernel Memory to delete the
 *        segment and kicks off a resumption sweep once it's freed.
 * @return false on failure.
 */
bool free_memory(t_syscall_memory* mem_free, t_queues* queues);

#pragma once

#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "kernel_memory/swap.h"
#include "utils/collections/list.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

/**
 * @brief The scheduler connection's request loop: NEW_PROCESS, MEM_ALLOC/
 *        FREE, address translation, reads/writes, SUSPEND/RESUME, until the
 *        scheduler disconnects or the module shuts down.
 * @param ptr  t_scheduler_data*, owned by this thread.
 */
void* listen_scheduler(void* ptr);

/** @brief Spawns a detached thread running listen_scheduler() for this
 * connection. */
void start_scheduler_listener(t_scheduler_data* scheduler_data);

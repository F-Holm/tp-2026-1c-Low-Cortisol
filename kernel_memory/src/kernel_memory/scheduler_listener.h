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

// The scheduler connection's request loop: NEW_PROCESS, MEM_ALLOC/FREE,
// address translation, reads/writes, SUSPEND/RESUME, until the scheduler
// disconnects or the module shuts down.
void* listen_scheduler(void* ptr);
void start_scheduler_listener(t_scheduler_data* scheduler_data);

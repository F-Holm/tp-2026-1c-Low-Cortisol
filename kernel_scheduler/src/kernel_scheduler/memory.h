#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

bool allocate_memory(t_syscall_memory* mem_alloc, t_queues* queues);
bool free_memory(t_syscall_memory* mem_free, t_queues* queues);

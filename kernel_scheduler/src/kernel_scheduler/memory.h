#ifndef KERNEL_SCHEDULER_MEMORY_H_
#define KERNEL_SCHEDULER_MEMORY_H_

#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/logger.h"
#include "utils/msg.h"

bool allocate_memory(t_syscall_memory* mem_alloc,t_colas* colas);
bool free_memory(t_syscall_memory* mem_free, t_colas* colas);

#endif /* KERNEL_SCHEDULER_MEMORY_H_ */

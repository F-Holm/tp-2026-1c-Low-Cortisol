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

bool allocate_memory(t_syscall_memory* mem_alloc, t_logger* logger,
                     t_socket_kernel_memory* socket_km, int socket_server);
bool free_memory(t_syscall_memory* mem_free, t_logger* logger,
                 t_socket_kernel_memory* socket_km, int socket_server);

#endif /* KERNEL_SCHEDULER_MEMORY_H_ */

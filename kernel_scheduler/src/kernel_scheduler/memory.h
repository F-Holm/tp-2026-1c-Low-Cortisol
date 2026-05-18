#ifndef KERNEL_SCHEDULER_MEMORY_H_
#define KERNEL_SCHEDULER_MEMORY_H_

#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/misc.h"
#include "utils/kernel_scheduler_cpu.h"

bool allocate_memory(t_syscall_memory* mem_alloc, t_logger* logger,
                     t_socket_kernel_memory* socket_km);
bool free_memory(t_syscall_memory* mem_free, t_logger* logger,
                 t_socket_kernel_memory* socket_km);

#endif /* KERNEL_SCHEDULER_MEMORY_H_ */

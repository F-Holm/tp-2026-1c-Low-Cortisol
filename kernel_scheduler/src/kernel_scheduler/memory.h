#ifndef KERNEL_SCHEDULER_MEMORY_H_
#define KERNEL_SCHEDULER_MEMORY_H_

#include <commons/log.h>
#include <sdtlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/misc.h"
#include "utils/kernel_scheduler_cpu.h"

int allocate_memory(syscall_memory* mem_alloc, t_logger* logger,
                    t_socket_kernel_memory* t_socket_km);
int free_memory(syscall_memory* mem_free, t_logger* logger,
                t_socket_kernel_memory* socket_km);

#endif /* KERNEL_SCHEDULER_MEMORY_H_ */

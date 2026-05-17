#ifndef KERNEL_SCHEDULER_MEMORY_H_
#define KERNEL_SCHEDULER_MEMORY_H_

#include <commons/log.h>
#include <sdtlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "kernel_scheduler/misc.h"
#include "utils/kernel_scheduler_cpu"

int allocate_memory(syscall_memory* mem_alloc, t_logger* logger,
                    t_socket_kernel_memory* t_socket_km);

#endif
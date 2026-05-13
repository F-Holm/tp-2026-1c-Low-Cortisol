#ifndef KERNEL_SCHEDULER_MEMORY_H_
#define KERNEL_SCHEDULER_MEMORY_H_

#include "utils/kernel_scheduler_cpu"
#include "kernel_scheduler/misc.h"
#include <stdio.h>
#include <sdtlib.h>
#include <stdint.h>
#include <commons/log.h>
#include <stdbool.h>

int allocate_memory(syscall_memory* mem_alloc, t_logger* logger, t_socket_kernel_memory* t_socket_km);



#endif
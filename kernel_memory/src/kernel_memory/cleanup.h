#pragma once

#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

void free_kernel_memory_data(t_kernel_memory_data* kernel_data);
void free_scheduler_data(t_scheduler_data* scheduler_data);
void free_cpu_data(t_cpu_data* cpu_data);
void close_cpu(t_cpu_data* cpu);
void free_stick_data(t_stick_data* stick_data);
void free_swap_data(t_swap_data* swap_data);
void free_process(t_process* process);
void free_main_memory(t_main_memory* memory);

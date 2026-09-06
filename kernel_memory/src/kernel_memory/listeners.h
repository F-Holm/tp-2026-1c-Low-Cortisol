#pragma once

#include <pthread.h>
#include <stdlib.h>

#include "configurator.h"
#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "kernel_memory/swap.h"
#include "protocol.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

void* listen_scheduler(void* ptr);
void* listen_cpu(void* ptr);
void* listen_swap(void* ptr);
void start_scheduler_listener(t_scheduler_data* scheduler_data);
void start_cpu_listener(t_cpu_data* cpu_data);
void start_swap_listener(t_swap_data* swap_data);

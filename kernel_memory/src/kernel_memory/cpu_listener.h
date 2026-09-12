#pragma once

#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

// The CPU connection's request loop: NEXT_INSTRUCTION, registers, memory
// reads/writes, until the CPU disconnects or the module shuts down.
void* listen_cpu(void* ptr);
void start_cpu_listener(t_cpu_data* cpu_data);

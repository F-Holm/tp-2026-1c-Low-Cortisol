#pragma once

#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/**
 * @brief The CPU connection's request loop: NEXT_INSTRUCTION, registers,
 *        memory reads/writes, until the CPU disconnects or the module shuts
 *        down.
 * @param ptr  t_cpu_data*, owned by this thread.
 */
void* listen_cpu(void* ptr);

/** @brief Spawns a detached thread running listen_cpu() for this CPU. */
void start_cpu_listener(t_cpu_data* cpu_data);

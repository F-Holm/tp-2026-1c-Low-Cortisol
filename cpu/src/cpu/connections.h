#pragma once

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"

bool connect_to_kernel_memory(t_cpu* cpu);
bool connect_to_kernel_scheduler(t_cpu* cpu);
bool connect_memory_stick(t_cpu* cpu);
uint32_t compute_offset(t_list* sticks);
bool handshake_memory_stick(t_cpu* cpu, int new_socket);
void notify_bsod(t_cpu* cpu);

#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registros_cpu.h"

void close_module(t_cpu* cpu);
void destroy_instruction(t_instruction* instruction);
void destroy_memory_stick(void* value);
void iterator_close_socket(void* value);

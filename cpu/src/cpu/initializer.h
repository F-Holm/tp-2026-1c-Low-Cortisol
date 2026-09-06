#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registros_cpu.h"

bool init_module(t_cpu* cpu, char* config_path);
bool check_arguments(int argc, char** argv);
void register_handlers(t_dictionary* handlers);

#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registros_cpu.h"

bool iniciar_modulo(t_cpu* cpu, char* path_config);
bool verificar_argumentos(int argc, char** argv);
void iniciar_diccionario(t_dictionary* handlers);

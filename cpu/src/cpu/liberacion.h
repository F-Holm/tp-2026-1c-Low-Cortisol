#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registros.h"

void cerrar_modulo(t_cpu* cpu);
void destruir_instruccion(t_instruccion* instrucion);
void destruir_memory_stick(void* value);
void iterator_close_socket(void* value);

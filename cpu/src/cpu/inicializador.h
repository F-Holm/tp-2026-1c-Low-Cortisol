#ifndef CPU_INICIALIZADOR_H_
#define CPU_INICIALIZADOR_H_

#include <commons/config.h>
#include <commons/log.h>

#include "cpu/cpu.h"
#include "utils/registros.h"

bool iniciar_modulo(t_cpu* cpu, char* path_config);
bool verificar_argumentos(int argc, char** argv);
void iniciar_diccionario(t_dictionary* handlers);
uint32_t calcular_offset(t_list* sticks);

#endif /* CPU_INICIALIZADOR_H_ */
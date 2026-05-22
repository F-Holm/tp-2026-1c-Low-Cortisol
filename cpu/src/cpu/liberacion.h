#ifndef CPU_LIBERACION_H_
#define CPU_LIBERACION_H_

#include <commons/config.h>
#include <commons/log.h>

#include "utils/registros.h"

#include "cpu/cpu.h"

void cerrar_modulo(t_cpu* cpu);
void destruir_instruccion(t_instruccion* instrucion);
void iterator_close_socket(void* value);


#endif /* CPU_LIBERACION_H_ */
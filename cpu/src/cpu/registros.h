#ifndef CPU_REGISTROS_H_
#define CPU_REGISTROS_H_

#include <stdio.h>

#include "utils/registros.h"

uint32_t get_registro(t_contexto* contexto, char* registro);
void set_registro(t_contexto* contexto, char* registro, uint32_t valor);

#endif /* CPU_REGISTROS_H_ */

#ifndef CPU_REGISTROS_H_
#define CPU_REGISTROS_H_

#include <stdio.h>

#include "utils/registros.h"

typedef struct
{
  t_registros* registros;
  t_list* tablaDeSegmentos;
} t_contexto;

uint32_t get_registro(t_registros* contexto, char* registro);
void set_registro(t_registros* contexto, char* registro, uint32_t valor);

#endif /* CPU_REGISTROS_H_ */

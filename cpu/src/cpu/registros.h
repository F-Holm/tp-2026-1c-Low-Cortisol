#pragma once

#include <stdio.h>

#include "utils/collections/list.h"
#include "utils/registros_cpu.h"

typedef struct
{
  t_registros* registros;
  t_list* tablaDeSegmentos;
  bool cambio_segmento;
} t_contexto;

uint32_t get_registro(t_registros* contexto, char* registro);
void set_registro(t_registros* contexto, char* registro, uint32_t valor);

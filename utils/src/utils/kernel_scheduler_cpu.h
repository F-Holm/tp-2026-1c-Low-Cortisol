#ifndef H_KERNEL_SCHEDULER_KERNEL_SCHEDULER_H_
#define H_KERNEL_SCHEDULER_KERNEL_SCHEDULER_H_
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/registros.h"

typedef enum
{
  NEW,
  READY,
  EXEC,
  BLOCK,
  SUSP_BLOCK,
  SUSP_READY,
  EXIT
} t_estado;

extern const char* const ESTADO_PROCESO[7];

typedef struct
{
  uint32_t pid;
  uint32_t tamanio_a_leer;
  uint32_t direccion_logica;
  void* buffer;
} t_peticion_stdin;

#endif /* H_KERNEL_SCHEDULER_KERNEL_SCHEDULER_H_ */
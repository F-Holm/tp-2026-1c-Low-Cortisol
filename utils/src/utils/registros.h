#ifndef UTILS_REGISTROS_H_
#define UTILS_REGISTROS_H_

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  uint8_t AX, BX, CX, DX;
  uint32_t PC, EAX, EBX, ECX, EDX, SI, DI;
} t_registros;

typedef struct
{
  uint32_t id;
  uint32_t pid;
  int base;
  int size;
} t_segmento;

#endif /* UTILS_REGISTROS_H_ */

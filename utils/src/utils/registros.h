#ifndef H_REGISTROS_H
#define H_REGISTROS_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct
{
  uint8_t AX, BX, CX, DX;
  uint32_t PC, EAX, EBX, ECX, EDX, SI, DI;

} t_contexto;

#endif /* H_REGISTROS_H */
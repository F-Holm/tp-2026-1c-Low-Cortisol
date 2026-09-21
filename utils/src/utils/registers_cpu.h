#pragma once

#include <stdint.h>

typedef struct
{
  uint8_t AX, BX, CX, DX;
  uint32_t PC, EAX, EBX, ECX, EDX, SI, DI;
} t_registers;

typedef struct
{
  uint32_t id;
  uint32_t pid;
  int base;
  int size;
} t_segment;

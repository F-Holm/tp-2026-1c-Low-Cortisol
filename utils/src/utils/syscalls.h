#pragma once

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/registers_cpu.h"

typedef struct
{
  uint32_t pid;
  uint32_t tamanio_a_leer;
  uint32_t direccion_logica;
} t_peticion_stdin;

typedef struct
{
  uint32_t pid;
  uint32_t tamanio_a_escribir;
  uint32_t direccion_logica;
} t_peticion_stdout;

typedef struct
{
  uint32_t pid;
  uint32_t tiempo_bloqueado;  // en milisegundos
} t_peticion_sleep;

typedef struct
{
  uint32_t pid;
  uint32_t id_segmento;
  uint32_t tamanio;
} t_syscall_memory;

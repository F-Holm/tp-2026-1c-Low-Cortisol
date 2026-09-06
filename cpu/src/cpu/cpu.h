#pragma once

#include <pthread.h>
#include <stdio.h>

#include "cpu/registros.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/registros.h"

typedef struct
{
  char* id;
  int socket_kernel_memory;
  int socket_kernel_scheduler;
  uint32_t tamanio_max_segmento;

  t_list* memory_sticks;
  t_dictionary* handlers;
  t_log* logger;
  t_config* config;
} t_cpu;

typedef struct
{
  char* nombre;
  char* parametros[3];
  int cantidad_parametros;
} t_instruccion;

typedef struct
{
  int socket_MS;
  uint32_t tamanio;
  uint32_t offset;
} t_memory_stick_info;

typedef enum
{
  BE_FALSE,
  BE_TRUE,
  BE_ERROR,
  BE_SIN_TABLA
} t_bool_extendido;

bool recibir_tamanio_maximo_segmento(t_cpu* cpu);
bool escuchar_kernel_memory(t_cpu* arg);
bool manejar_paquete(t_cpu* cpu, t_list* lista_paquete, char ip_stick[16],
                     char puerto_stick[6], uint32_t* tamanio);
void manejo_instrucciones(t_cpu* cpu);
uint32_t recibir_pid_kernel_scheduler(t_cpu* cpu);
bool pedir_contexto_kernel_memory(t_cpu* cpu, uint32_t pid);
t_registros* recibir_contexto_kernel_memory(t_cpu* cpu);
t_list* recibir_tabla_segmentos(t_cpu* cpu, t_contexto* contexto);
bool ejecutar_ciclo_instruccion(t_cpu* cpu, uint32_t pid, t_contexto* contexto);
char* etapa_fetch(t_cpu* cpu, uint32_t pid, uint32_t pc);
bool pedir_instruccion_kernel_memory(t_cpu* cpu, uint32_t pid, uint32_t pc);
char* recibir_instruccion_kernel_memory(t_cpu* cpu);
t_instruccion* etapa_decode(char* instruccion_KM);
t_bool_extendido etapa_execute(t_cpu* cpu, t_contexto* contexto,
                               t_instruccion* instruccion, uint32_t pid);
t_bool_extendido check_interrupt(t_cpu* cpu, uint32_t pid);
bool enviar_contexto_actualizado(t_cpu* cpu, uint32_t pid,
                                 t_registros* contexto_actualizado);
bool actualizar_tabla_segmentos(t_cpu* cpu, uint32_t pid, t_contexto* contexto);

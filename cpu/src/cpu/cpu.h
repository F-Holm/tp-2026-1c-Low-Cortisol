#ifndef CPU_CPU_H_
#define CPU_CPU_H_

#include <commons/collections/list.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdio.h>

#include "utils/client.h"
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

void recibir_tamanio_maximo_segmento(t_cpu* cpu);
void escuchar_kernel_memory(t_cpu* arg);
bool manejar_paquete(t_cpu* cpu, t_list* lista_paquete, char ip_stick[16],
                     char puerto_stick[6]);
void manejo_instrucciones(t_cpu* cpu);
uint32_t recibir_pid_kernel_scheduler(t_cpu* cpu);
bool pedir_contexto_kernel_memory(t_cpu* cpu, uint32_t pid);
t_registros* recibir_contexto_kernel_memory(t_cpu* cpu);
void ejecutar_ciclo_instruccion(t_cpu* cpu, uint32_t pid, t_contexto* contexto);
char* etapa_fetch(t_cpu* cpu, uint32_t pid, uint32_t pc);
void pedir_instruccion_kernel_memory(t_cpu* cpu, uint32_t pid, uint32_t pc);
char* recibir_instruccion_kernel_memory(t_cpu* cpu);
t_instruccion* etapa_decode(char* instruccion_KM);
bool etapa_execute(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid);
bool check_interrupt(t_cpu* cpu, uint32_t pid, t_contexto* contexto);
void enviar_contexto_actualizado(t_cpu* cpu, uint32_t pid,
                                 t_registros* contexto_actualizado);

#endif /* CPU_CPU_H_ */

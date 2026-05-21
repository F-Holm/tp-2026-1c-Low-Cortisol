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
  pthread_t kernel_memory_hilo;
  pthread_t kernel_scheduler_hilo;
} t_hilo_cpu;

typedef struct
{
  char* id;

  int socket_kernel_memory;
  int socket_kernel_scheduler;

  t_list* memory_sticks;

  t_hilo_cpu hilos;

  t_log* logger;
  t_config* config;
} t_cpu;


typedef struct {
    char* nombre;
    char* parametros[3];
    int cantidad_parametros;
} t_instruccion;

typedef enum 
{
  I_NOOP,
  I_SET,
  I_MOV_IN,
  I_MOV_OUT,
  I_SUM,
  I_SUB,
  I_JNZ,
  I_COPY_MEM,

} t_instruccines;


void iniciar_hilo_kernel_memory(void* arg);
void iniciar_hilo_kernel_scheduler(void* arg);
void* escuchar_kernel_memory(void* arg);
bool iniciar_conexion_kmemory(t_cpu* cpu);
bool iniciar_conexion_scheduler(t_cpu* cpu);
bool conexion_memory_stick(t_cpu* cpu, int nuevo_socket);
bool iniciar_modulo(t_cpu* cpu, char* path_config);
bool verificar_argumentos(int argc, char** argv);
void cerrar_modulo(t_cpu* cpu);

#endif /* CPU_CPU_H_ */

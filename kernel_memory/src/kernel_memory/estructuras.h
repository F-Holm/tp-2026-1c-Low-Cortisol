#pragma once

#include <pthread.h>

#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/registros.h"

typedef enum
{
  BEST,
  WORST
} t_allocation_strategy;

typedef struct
{
  int compaction_delay;
  int tamanio_total;
  int tamanio_maximo_segmento;
  t_list* segmentos;
  t_list* huecos;
  int allocation_strategy;
  pthread_mutex_t* mutex_memoria_principal;
} t_memoria_principal;

typedef struct
{
  int socket_swap;
  t_log* logger;
  t_list* lista_bloques;
  int tamanio_swap;
  int tamanio_bloque;
} t_datos_swap;

typedef struct
{
  int socket_kernel_memory;
  int instruction_delay;
  int compaction_delay;
  int segment_max_size;
  t_allocation_strategy allocation_strategy;
  int socket_scheduler;
  char* scripts_basepath;
  t_log* logger;
  t_list* sticks_conectados;
  t_list* cpus_conectados;
  t_list* procesos;
  t_memoria_principal* memoria_principal;
  t_datos_swap* datos_swap;
  pthread_mutex_t* mutex_procesos;
  pthread_mutex_t* mutex_lista_sockets;
  int hilos_activos;
  pthread_mutex_t* mutex_hilos_activos;
  pthread_cond_t* cond_hilos_activos;
} t_datos_kernel_mem;

typedef struct
{
  int socket_kernel_memory;
  int socket_scheduler;
  t_log* logger;
  t_list* procesos;
  pthread_mutex_t* mutex_procesos;
  char* scripts_basepath;
  t_list* sticks_conectados;
  pthread_mutex_t* mutex_lista_sockets;
  t_memoria_principal* memoria_principal;
  t_datos_swap* datos_swap;
  int* hilos_activos;
  pthread_mutex_t* mutex_hilos_activos;
  pthread_cond_t* cond_hilos_activos;
} t_datos_scheduler;

typedef struct
{
  int id;
  int socket_cpu;
  int socket_scheduler;
  t_log* logger;
  int instruction_delay;
  t_list* procesos;
  pthread_mutex_t* mutex_procesos;
  t_memoria_principal* memoria_principal;
  int* hilos_activos;
  pthread_mutex_t* mutex_hilos_activos;
  pthread_cond_t* cond_hilos_activos;
} t_datos_cpu;

typedef struct
{
  int tamanio_stick;
  int socket_stick;
  char ip_memory_stick[16];
  int puerto_stick;
  int socket_scheduler;
  t_log* logger;
} t_datos_stick;

typedef struct
{
  uint32_t pid;
  char* path_instrucciones;
  char** instrucciones;
  int cant_instrucciones;
  t_list* segmentos;
  t_registros registro;
} t_proceso;

typedef struct
{
  int base;
  int size;
} t_hueco;

typedef struct
{
  int num_bloque;
  uint32_t pid;
  int num_segmento;
  int num_bloque_del_segmento;
  int tamanio_segmento;
} t_datos_bloque;

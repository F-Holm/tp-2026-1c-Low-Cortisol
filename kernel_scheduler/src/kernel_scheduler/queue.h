#ifndef KERNEL_SCHEDULER_QUEUE_H_
#define KERNEL_SCHEDULER_QUEUE_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/misc.h"

typedef struct
{
  t_queue* cola;
  pthread_mutex_t mutex_cola;
} t_cola;

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
} t_lista;

typedef struct
{
  t_queue* cola;
  pthread_mutex_t mutex_cola;
  int algoritmo;
} t_cola_individual_ready;

typedef struct
{
  int cantidad_colas;
  t_cola_individual_ready* colas;
} t_cola_ready;

typedef struct
{
  t_queue* cola;
  pthread_mutex_t mutex_cola;
  t_pcb* priordad_mas_baja;
  int quantum;     // = 0 si no es RR
  bool desalojo;   // si el desalojo está habilitado
} t_cola_execute;  // como algunos valores no cambian nunca (quantum y
                   // desalojo), no necesitan mutex

typedef struct
{
  t_cola new;
  t_cola_ready ready;
  t_cola_execute exec;
  t_cola block;
  t_lista susp_block;
  t_lista susp_ready;
  t_cola exit;
} t_colas;

// ingresar NULL en t_list si no es CMN
// ingresar quantum = 0 si no es RR
void inicializar_colas(t_colas* colas, int algoritmo, t_list* algoritmos_cmn,
                       int quantum, bool desalojo);
void destruir_colas(t_colas* colas);

#endif /* KERNEL_SCHEDULER_QUEUE_H_ */

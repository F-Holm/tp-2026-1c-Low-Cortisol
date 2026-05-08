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

bool puedo_suspender(t_pcb* pcb);
void cambio_new_ready(t_pcb* pcb, t_cola* new, t_cola_ready* ready,
                      t_logger* logger);
void cambio_ready_exec(t_pcb* pcb, t_cola_ready* ready, t_cola_execute* exec,
                       t_logger* logger);
void cambio_exec_exit(t_pcb* pcb, t_cola_execute* exec, t_cola exit,
                      t_logger* logger);
void cambio_exec_block(t_pcb* pcb, t_cola_execute* exec, t_cola block,
                       t_logger* logger);
void cambio_block_ready(t_pcb* pcb, t_cola* block, t_cola_ready* ready,
                        t_logger* logger);
void cambio_block_susp_block(t_pcb* pcb, t_cola* block, t_lista* susp_block,
                             t_logger* logger);
void cambio_susp_block_block(t_pcb* pcb, t_lista* susp_block, t_cola* block,
                             t_logger* logger);
void cambio_susp_block_susp(t_pcb* pcb, t_lista* susp_block,
                            t_lista* susp_ready, t_logger* logger);
void cambio_susp_ready(t_pcb* pcb, t_lista* susp_rady, t_cola_ready* ready,
                       t_logger* logger);

#endif /* KERNEL_SCHEDULER_QUEUE_H_ */

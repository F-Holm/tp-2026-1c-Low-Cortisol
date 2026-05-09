#ifndef KERNEL_SCHEDULER_QUEUE_H_
#define KERNEL_SCHEDULER_QUEUE_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/misc.h"

typedef enum
{
  EST_NEW,
  EST_READY,
  EST_EXEC,
  EST_BLOCK,
  EST_SUSP_BLOCK,
  EST_SUSP_READY,
  EXIT
} t_estados;

extern const char* const ESTADOS_STR[7];

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
  bool cola_multi_nivel;
} t_cola_ready;

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
  t_pcb* priordad_mas_baja;
  int quantum;      // = 0 si no es RR
  bool desalojo;    // si el desalojo está habilitado
} t_lista_execute;  // como algunos valores no cambian nunca (quantum y
                    // desalojo), no necesitan mutex

typedef struct
{
  t_cola new;
  t_cola_ready ready;
  t_lista_execute exec;
  t_lista block;
  t_lista susp_block;
  t_lista susp_ready;
  t_cola exit;
} t_colas;

// ingresar NULL en t_list si no es CMN
// ingresar quantum = 0 si no es RR
void inicializar_colas(t_colas* colas, int algoritmo, t_list* algoritmos_cmn,
                       int quantum, bool desalojo);
void destruir_colas(t_colas* colas);

void log_cambio_estado(t_logger* logger, uint32_t pid, int estado_anterior,
                       int estado_nuevo);
bool esta_suspendido(t_pcb* pcb);
bool puedo_suspender(t_pcb* pcb);
void set_tiempo_bloqueado(t_pcb* pcb, unsigned long tiempo);
void update_priordad_mas_baja_exec(t_lista_execute* exec);

void cambio_a_new(t_pcb* pcb, t_cola* new);
void cambio_a_ready(t_pcb* pcb, t_cola_ready* ready, t_logger* logger);
void cambio_a_exec(t_pcb* pcb, t_lista_execute* exec);
void cambio_a_block(t_pcb* pcb, t_cola block);
void cambio_a_susp_block(t_pcb* pcb, t_lista* susp_block);
void cambio_a_susp_ready(t_pcb* pcb, t_lista* susp_ready);
void cambio_a_exit(t_pcb* pcb, t_cola exit);

t_pcb* cambio_sacar_new(t_cola* new);
t_pcb* cambio_sacar_ready(t_cola_ready* ready);
void cambio_sacar_exec(t_pcb* pcb, t_lista_execute* exec);
void cambio_sacar_block(t_pcb* pcb, t_lista* block);
void cambio_sacar_susp_block(t_pcb* pcb, t_lista* susp_block);
void cambio_sacar_susp_ready(t_pcb* pcb, t_lista* susp_ready);
t_pcb* cambio_sacar_exit(t_cola* exit);

void cambio_new_ready(t_cola* new, t_cola_ready* ready, t_logger* logger);
// No implementado, solo contiene el log por ahora. Usar funciones individuales
void cambio_ready_exec(t_pcb* pcb, t_lista_execute* exec, t_logger* logger);
void cambio_exec_ready(t_pcb* pcb, t_lista_execute* exec, t_cola_ready* ready,
                       t_logger* logger);
void cambio_exec_exit(t_pcb* pcb, t_lista_execute* exec, t_cola exit,
                      t_logger* logger);
void cambio_exec_block(t_pcb* pcb, t_lista_execute* exec, t_cola block,
                       t_logger* logger);
void cambio_block_ready(t_pcb* pcb, t_cola* block, t_cola_ready* ready,
                        t_logger* logger);
void cambio_block_susp_block(t_pcb* pcb, t_cola* block, t_lista* susp_block,
                             t_logger* logger);
void cambio_susp_block_block(t_pcb* pcb, t_lista* susp_block, t_cola* block,
                             t_logger* logger);
void cambio_susp_block_susp_ready(t_pcb* pcb, t_lista* susp_block,
                                  t_lista* susp_ready, t_logger* logger);
void cambio_susp_ready_ready(t_pcb* pcb, t_lista* susp_ready,
                             t_cola_ready* ready, t_logger* logger);
void cambio_desbloquear(t_pcb* pcb, t_cola* block, t_lista* susp_block,
                        t_lista* susp_ready, t_cola_ready* ready,
                        t_logger* logger);

#endif /* KERNEL_SCHEDULER_QUEUE_H_ */

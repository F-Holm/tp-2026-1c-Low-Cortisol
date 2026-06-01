#ifndef KERNEL_SCHEDULER_SUSPENDIDO_H_
#define KERNEL_SCHEDULER_SUSPENDIDO_H_

#include <pthread.h>

#include "kernel_scheduler/queue.h"
#include "utils/logger.h"

typedef enum
{
  EH_EJECUTANDO,
  EH_ESPERANDO_PROCESO,
  EH_BLOQUEADO,
  EH_FINALIZANDO,
  EH_FINALIZADO
} t_estado_hilo;

typedef struct
{
  pthread_t hilo;
  pthread_mutex_t mutex_estado;
  int estado;
  pthread_mutex_t* mutex_suspender_des_suspender;
  pthread_cond_t* esperar_proceso;
  pthread_cond_t desbloquear;
  t_cola_ready* ready;
  t_lista* block;
  t_lista* susp_block;
  t_lista* susp_ready;
  t_logger* logger;
} t_datos_hilo_suspendido;

typedef struct
{
  t_datos_hilo_suspendido* datos;
  int suspension_timeout;
} t_datos_hilo_suspensor;

typedef struct
{
  t_datos_hilo_suspendido* datos;
} t_datos_hilo_des_suspensor;

void iniciar_hilos_suspendido(
    t_colas* colas, int suspension_timeout,
    t_datos_hilo_suspensor** datos_hilo_suspensor,
    t_datos_hilo_des_suspensor** datos_hilo_des_suspensor);

void terminar_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

void destruir_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

void bloquear_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

void desbloquear_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

#endif /* KERNEL_SCHEDULER_SUSPENDIDO_H_ */

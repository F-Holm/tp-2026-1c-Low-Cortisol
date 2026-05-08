#ifndef KERNEL_SCHEDULER_QUEUE_H_
#define KERNEL_SCHEDULER_QUEUE_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/kernel_scheduler.h"

typedef struct
{
  t_queue* cola;
  pthread_mutex_t mutex_cola;
} t_cola;

typedef struct
{
  int cantidad_colas;
  t_cola** colas;
} t_cola_ready;

typedef struct
{
  t_cola* cola_execute;
  t_pcb* priordad_mas_baja;
} t_cola_execute;

#endif /* KERNEL_SCHEDULER_QUEUE_H_ */
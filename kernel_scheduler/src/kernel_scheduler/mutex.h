#ifndef KERNEL_SCHEDULER_MUTEX_H_
#define KERNEL_SCHEDULER_MUTEX_H_

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"

typedef struct
{
  char* id;
  int prioridad_original_proceso;
  pthread_mutex_t mutex;
  bool prioridad_activa;
  t_list* lista;
  t_pcb* proceso_actual;
  int estado;
  t_logger* logger;
  t_colas* colas;
} t_mutex;

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger* logger);
void mutex_lock(t_mutex* mutex, t_pcb* pcb);
void mutex_unlock(t_mutex* mutex, t_pcb* pcb);
void destroy_mutex(t_mutex* mutex);

#endif /* KERNEL_SCHEDULER_MUTEX_H_ */

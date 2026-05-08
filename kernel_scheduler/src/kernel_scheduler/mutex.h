#ifndef KERNEL_SCHEDULER_MUTEX_H_
#define KERNEL_SCHEDULER_MUTEX_H_

#include <commons/collections/list.h>

#include "kernel_scheduler/misc.h"

typedef struct
{
  char* id;
  int prioridad_original_proceso;
  pthread_mutex_t mutex;
  bool prioridad_activa;
  t_list* lista;
  t_pcb* proceso_actual;
  int estado;
  t_logger logger;
} t_mutex;

#endif /* KERNEL_SCHEDULER_MUTEX_H_ */

#ifndef KERNEL_SCHEDULER_MUTEX_H_
#define KERNEL_SCHEDULER_MUTEX_H_

#include <commons/collections/dictionary.h>
#include <commons/collections/list.h>
#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/logger.h"

typedef struct
{
  char* id;
  int prioridad_siguiente;
  pthread_mutex_t mutex;
  bool prioridad_activa;
  t_list* lista;
  t_pcb* proceso_actual;
  int estado;
  t_colas* colas;
} t_mutex;

typedef struct
{
  t_dictionary* lista;
  pthread_mutex_t mutex_lista;
} t_lista_mutex;

typedef enum
{
  RM_MUTEX_CREADO,
  RM_NOMBRE_MUTEX_YA_EXISTE,
  RM_NOMBRE_MUTEX_NO_EXISTE,
  RM_MUTEX_BLOQUEADO,
  RM_ESPERANDO_MUTEX,
  RM_MUTEX_DESBLOQUEADO,
  RM_PROCESO_NO_TIENE_MUTEX_BLOQUEADO
} t_return_mutex;

t_lista_mutex* inicializar_lista_mutex(void);
void destruir_lista_mutex(t_lista_mutex* lista_mutex);

int crear_y_add_mutex(t_lista_mutex* lista_mutex, char* id,
                      bool prioridad_activa, t_colas* colas);
int lista_mutex_lock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb);
int lista_mutex_unlock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb);

#endif /* KERNEL_SCHEDULER_MUTEX_H_ */

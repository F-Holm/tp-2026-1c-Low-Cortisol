#ifndef KERNEL_SCHEDULER_MUTEX_H_
#define KERNEL_SCHEDULER_MUTEX_H_

#include <commons/collections/list.h>
#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"
#include "utils/logger.h"

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

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
} t_lista_mutex;

t_lista_mutex* inicializar_lista_mutex(void);
void destruir_lista_mutex(t_lista_mutex* lista_mutex);

void crear_y_add_mutex(t_lista_mutex* lista_mutex, char* id,
                       bool prioridad_activa, t_logger* logger);
bool lista_mutex_lock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb);
bool lista_mutex_unlock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb);

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger* logger);

// Devuelve true si el proceso puede usar el recurso directamente sin ser
// bloqueado El proceso se bloquea automáticamente si devuelve false
bool mutex_lock(t_mutex* mutex, t_pcb* pcb);

// Devuelve false si el proceso que libera el mutex no es el que lo bloqueo
bool mutex_unlock(t_mutex* mutex, t_pcb* pcb);

void destroy_mutex(t_mutex* mutex);

#endif /* KERNEL_SCHEDULER_MUTEX_H_ */

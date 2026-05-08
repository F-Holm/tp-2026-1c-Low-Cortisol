#include "kernel_scheduler/mutex.h"

#include <string.h>

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger logger)
{
  t_mutex mutex = malloc(sizeof(t_mutex));
  mutex->id = malloc(strlen(id) + 1);
  strcpy(mutex->id, id);
  mutex->prioridad_original_proceso = -1;
  pthread_mutex_init(&(mutex->mutex));
  mutex->prioridad_activa = prioridad_activa;
  mutex->lista = list_create();
  mutex->proceso_actual = NULL;
  mutex->estado = 1;
  mutex->logger = logger;
}

void mutex_lock(t_mutex* mutex, t_cola_ready* cola_ready, t_cola* cola_block);

void mutex_unlock(t_mutex* mutex, t_cola_ready* cola_ready, t_cola* cola_block);

void destroy_mutex(t_mutex* mutex)
{
  free(mutex->id);
  pthread_mutex_destroy(&(mutex->mutex));
  list_destroy(mutex->lista);
  free(mutex);
}

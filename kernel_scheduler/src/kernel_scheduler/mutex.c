#include "kernel_scheduler/mutex.h"

#include <string.h>

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger* logger)
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

void mutex_lock(t_mutex* mutex, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex->mutex));
  if (mutex->estado == 1)
  {
    pthread_mutex_lock(&(pcb_mutex_pcb));
    mutex->prioridad_original_proceso = pcb->prioridad;
    pthread_mutex_unlock(&(pcb_mutex_pcb));
    mutex->proceso_actual == pcb;
  }
  else if (mutex->prioridad_activa)
  {
    cambio_exec_block(pcb, mutex->colas->exec, mutex->colas->block,
                      mutex->logger);
  }
  else
  {
    list_add(mutex->lista, pcb);
    cambio_exec_block(pcb, mutex->colas->exec, mutex->colas->block,
                      mutex->logger);
  }
  estado--;
  pthread_mutex_unlock(&(mutex->mutex));
}

void mutex_unlock(t_mutex* mutex, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex->mutex));
  if (mutex->estado == 1)
  {
    pthread_mutex_lock(&(pcb_mutex_pcb));
    mutex->prioridad_original_proceso = pcb->prioridad;
    pthread_mutex_unlock(&(pcb_mutex_pcb));
    mutex->proceso_actual == pcb;
  }
  else
  {
    t_pcb* new_pcb = list_remove(mutex->lista, 0);
    cambio_exec_block(new_pcb, mutex->colas->exec, mutex->colas->block,
                      mutex->logger);
  }
  estado++;
  pthread_mutex_unlock(&(mutex->mutex));
}

void destroy_mutex(t_mutex* mutex)
{
  pthread_mutex_lock(&(mutex->mutex));
  t_list_iterator* iterador_lista = list_iterator_create(mutex_lista);
  while (list_iterator_has_next(iterador_lista))
  {
    t_pcb* pcb = list_iterator_next(iterador_lista);
    list_iterator_remove(iterador_lista);
    desbloquear(pcb, mutex->colas->block, mutex->colas->susp_block,
                mutex->colas->susp_ready, mutex->colas->ready, mutex->logger);
  }
  list_iterator_destroy(iterador_lista);
  free(mutex->id);
  pthread_mutex_unlock(&(mutex->mutex));
  pthread_mutex_destroy(&(mutex->mutex));
  list_destroy(mutex->lista);
  free(mutex);
}

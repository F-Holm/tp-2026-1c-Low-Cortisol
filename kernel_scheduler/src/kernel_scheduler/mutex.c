#include "kernel_scheduler/mutex.h"

#include <string.h>

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
} t_lista_mutex;

t_lista_mutex* inicializar_lista_mutex(void)
{
  t_lista_mutex* lista_mutex = malloc(sizeof(t_lista_mutex));
  lista_mutex->lista = list_create();
  pthread_mutex_init(&(lista_mutex->mutex_lista));
  return lista_mutex;
}

void destruir_lista_mutex(t_lista_mutex* lista_mutex)
{
  list_destroy_and_destroy_elements(lista_mutex->lista, destroy_mutex);
  pthread_mutex_destroy(&(lista_mutex->mutex_lista));
}

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger* logger)
{
  t_mutex* mutex = malloc(sizeof(t_mutex));
  mutex->id = malloc(strlen(id) + 1);
  strcpy(mutex->id, id);
  mutex->prioridad_original_proceso = -1;
  pthread_mutex_init(&(mutex->mutex), NULL);
  mutex->prioridad_activa = prioridad_activa;
  mutex->lista = list_create();
  mutex->proceso_actual = NULL;
  mutex->estado = 1;
  mutex->logger = logger;
  return mutex;
}

bool mutex_lock(t_mutex* mutex, t_pcb* pcb)
{
  bool ret = false;
  int prioridad_pcb = get_prioridad_pcb(pcb);
  pthread_mutex_lock(&(mutex->mutex));
  if (mutex->estado == 1)
  {
    mutex->prioridad_original_proceso = prioridad_pcb;
    mutex->proceso_actual = pcb;
    ret = true;
  }
  else if (mutex->prioridad_activa && mutex->estado < 0)
  {
    t_list_iterator* iterador_lista = list_iterator_create(mutex->lista);
    while (true)
    {
      if (!list_iterator_next(iterador_lista))
      {
        list_add(mutex->lista, pcb);
        break;
      }
      t_pcb* aux = list_iterator_next(iterador_lista);
      int prioridad_aux = get_prioridad_pcb(aux);
      if (prioridad_aux > prioridad_pcb)
      {
        list_iterator_replace(iterador_lista, pcb);
        list_iterator_add(iterador_lista, aux);
        break;
      }
    }
    list_iterator_destroy(iterador_lista);
    cambio_exec_block(pcb, &(mutex->colas->exec), &(mutex->colas->block),
                      mutex->logger);
  }
  else
  {
    list_add(mutex->lista, pcb);
    cambio_exec_block(pcb, &(mutex->colas->exec), &(mutex->colas->block),
                      mutex->logger);
  }
  mutex->estado--;
  pthread_mutex_unlock(&(mutex->mutex));
  return ret;
}

bool mutex_unlock(t_mutex* mutex, t_pcb* pcb)
{
  pthread_mutex_lock(&(mutex->mutex));
  if (pcb != mutex->proceso_actual)
  {
    pthread_mutex_unlock(&(mutex->mutex));
    return false;
  }
  if (mutex->prioridad_activa)
  {
    pthread_mutex_lock(&(pcb->mutex_pcb));
    pcb->prioridad = mutex->prioridad_original_proceso;
    pthread_mutex_unlock(&(pcb->mutex_pcb));
  }
  cambio_desbloquear(pcb, &(mutex->colas->block), &(mutex->colas->susp_block),
                     &(mutex->colas->susp_ready), &(mutex->colas->ready),
                     mutex->logger);
  if (mutex->estado == 0)
  {
    mutex->proceso_actual = NULL;
    mutex->prioridad_original_proceso = 0;
  }
  else
  {
    mutex->proceso_actual = list_remove(mutex->lista, 0);
    if (mutex->prioridad_activa)
    {
      mutex->prioridad_original_proceso =
          get_prioridad_pcb(mutex->proceso_actual);
    }
    cambio_exec_block(mutex->proceso_actual, &(mutex->colas->exec),
                      &(mutex->colas->block), mutex->logger);
  }
  mutex->estado++;
  pthread_mutex_unlock(&(mutex->mutex));
  return true;
}

void destroy_mutex(t_mutex* mutex)
{
  pthread_mutex_lock(&(mutex->mutex));
  t_list_iterator* iterador_lista = list_iterator_create(mutex->lista);
  while (list_iterator_has_next(iterador_lista))
  {
    t_pcb* pcb = list_iterator_next(iterador_lista);
    list_iterator_remove(iterador_lista);
    cambio_desbloquear(pcb, &(mutex->colas->block), &(mutex->colas->susp_block),
                       &(mutex->colas->susp_ready), &(mutex->colas->ready),
                       mutex->logger);
  }
  list_iterator_destroy(iterador_lista);
  free(mutex->id);
  pthread_mutex_unlock(&(mutex->mutex));
  pthread_mutex_destroy(&(mutex->mutex));
  list_destroy(mutex->lista);
  free(mutex);
}

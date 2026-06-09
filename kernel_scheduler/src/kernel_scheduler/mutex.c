#include "kernel_scheduler/mutex.h"

#include <stdint.h>
#include <string.h>

static void log_mutex_tomado(t_logger* logger, uint32_t pid, char* id_mutex)
{
  logger_info(logger, "## %u Toma el Mutex %s", pid, id_mutex);
}

static void log_mutex_liberado(t_logger* logger, uint32_t pid, char* id_mutex)
{
  logger_info(logger, "## %u Toma el Mutex %s", pid, id_mutex);
}

static void log_cambio_de_prioridad(t_logger* logger, uint32_t pid,
                                    int prioridad_anterior, int prioridad_nueva)
{
  logger_info(logger, "## %u Cambio de prioridad: %d - %d", pid,
              prioridad_anterior, prioridad_nueva);
}

t_lista_mutex* inicializar_lista_mutex(void)
{
  t_lista_mutex* lista_mutex = malloc(sizeof(t_lista_mutex));
  lista_mutex->lista = list_create();
  pthread_mutex_init(&(lista_mutex->mutex_lista), NULL);
  return lista_mutex;
}

static void destroy_mutex_iterator(void* mutex)
{
  destroy_mutex(mutex);
}

void destruir_lista_mutex(t_lista_mutex* lista_mutex)
{
  list_destroy_and_destroy_elements(lista_mutex->lista, destroy_mutex_iterator);
  pthread_mutex_destroy(&(lista_mutex->mutex_lista));
  free(lista_mutex);
}

void crear_y_add_mutex(t_lista_mutex* lista_mutex, char* id,
                       bool prioridad_activa, t_logger* logger)
{
  pthread_mutex_lock(&(lista_mutex->mutex_lista));
  list_add(lista_mutex->lista, crear_mutex(id, prioridad_activa, logger));
  pthread_mutex_unlock(&(lista_mutex->mutex_lista));
}

static t_mutex* get_mutex_by_id(t_lista_mutex* lista_mutex, char* id)
{
  bool encontro = false;
  t_mutex* mutex;
  pthread_mutex_lock(&(lista_mutex->mutex_lista));
  t_list_iterator* iterador_mutex = list_iterator_create(lista_mutex->lista);
  while (list_iterator_has_next(iterador_mutex))
  {
    mutex = list_iterator_next(iterador_mutex);
    if (strcmp(mutex->id, id) == 0)
    {
      encontro = true;
      break;
    }
  }
  list_iterator_destroy(iterador_mutex);
  pthread_mutex_unlock(&(lista_mutex->mutex_lista));
  return encontro ? mutex : NULL;
}

bool lista_mutex_lock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb)
{
  return mutex_lock(get_mutex_by_id(lista_mutex, id), pcb);
}

bool lista_mutex_unlock(t_lista_mutex* lista_mutex, char* id, t_pcb* pcb)
{
  return mutex_unlock(get_mutex_by_id(lista_mutex, id), pcb);
}

t_mutex* crear_mutex(char* id, bool prioridad_activa, t_logger* logger)
{
  t_mutex* mutex = malloc(sizeof(t_mutex));
  mutex->id = malloc(strlen(id) + 1);
  strcpy(mutex->id, id);
  mutex->prioridad_siguiente = -1;
  pthread_mutex_init(&(mutex->mutex), NULL);
  mutex->prioridad_activa = prioridad_activa;
  mutex->lista = list_create();
  mutex->proceso_actual = NULL;
  mutex->estado = 1;
  mutex->logger = logger;
  return mutex;
}

static void eliminar_prioridad_lista(t_list* lista, int prioridad)
{
  t_list_iterator* iterador_lista = list_iterator_create(lista);
  while (list_iterator_has_next(iterador_lista))
  {
    int* temp = (int*)list_iterator_next(iterador_lista);
    if (*temp == prioridad)
    {
      list_iterator_remove(iterador_lista);
      free(temp);
      break;
    }
  }
  list_iterator_destroy(iterador_lista);
}

static bool mayorPrioridadQue(void* p1, void* p2)
{
  return *(int*)p1 < *(int*)p2;
}

static void insertar_prioridad(t_pcb* pcb, int prioridad, t_logger* logger)
{
  int* aux = malloc(sizeof(int));
  *aux = prioridad;
  pthread_mutex_lock(&(pcb->mutex_prioridad));
  list_add_sorted(pcb->lista_prioridades, aux, mayorPrioridadQue);
  if (prioridad < pcb->prioridad)
  {
    log_cambio_de_prioridad(logger, pcb->pid, pcb->prioridad, prioridad);
    pcb->prioridad = prioridad;
  }
  pthread_mutex_unlock(&(pcb->mutex_prioridad));
}

static void reemplazar_prioridad(t_pcb* pcb, int prioridad_vieja,
                                 int prioridad_nueva, t_logger* logger)
{
  int* aux = malloc(sizeof(int));
  *aux = prioridad_nueva;
  pthread_mutex_lock(&(pcb->mutex_prioridad));
  eliminar_prioridad_lista(pcb->lista_prioridades, prioridad_vieja);
  list_add_sorted(pcb->lista_prioridades, aux, mayorPrioridadQue);
  int prioridad = *(int*)list_get(pcb->lista_prioridades, 0);
  if (prioridad != pcb->prioridad)
  {
    log_cambio_de_prioridad(logger, pcb->pid, pcb->prioridad, prioridad);
    pcb->prioridad = prioridad;
  }
  pthread_mutex_unlock(&(pcb->mutex_prioridad));
}

static void eliminar_prioridad(t_pcb* pcb, int prioridad, t_logger* logger)
{
  pthread_mutex_lock(&(pcb->mutex_prioridad));
  eliminar_prioridad_lista(pcb->lista_prioridades, prioridad);

  int nueva_prioridad = *(int*)list_get(pcb->lista_prioridades, 0);
  if (pcb->prioridad != nueva_prioridad)
  {
    log_cambio_de_prioridad(logger, pcb->pid, pcb->prioridad, nueva_prioridad);
    pcb->prioridad = nueva_prioridad;
  }

  pthread_mutex_unlock(&(pcb->mutex_prioridad));
}

bool mutex_lock(t_mutex* mutex, t_pcb* pcb)
{
  bool ret = false;
  int prioridad_pcb = get_prioridad_pcb(pcb);
  pthread_mutex_lock(&(mutex->mutex));
  if (mutex->estado == 1)
  {
    log_mutex_tomado(mutex->logger, pcb->pid, mutex->id);
    mutex->proceso_actual = pcb;
    insertar_prioridad(pcb, -1, mutex->logger);
    ret = true;
  }
  else if (mutex->prioridad_activa)
  {
    if (insertar_pcb_en_orden(mutex->lista, pcb) == 0)
    {
      reemplazar_prioridad(mutex->proceso_actual, mutex->prioridad_siguiente,
                           prioridad_pcb, mutex->logger);
    }
    cambio_exec_block(pcb, mutex->colas);
  }
  else
  {
    list_add(mutex->lista, pcb);
    cambio_exec_block(pcb, mutex->colas);
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
    eliminar_prioridad(pcb, mutex->prioridad_siguiente, mutex->logger);
  }
  log_mutex_liberado(mutex->logger, pcb->pid, mutex->id);
  if (mutex->estado == 0)
  {
    mutex->proceso_actual = NULL;
    mutex->prioridad_siguiente = -1;
  }
  else if (mutex->estado < 0)
  {
    mutex->proceso_actual = list_remove(mutex->lista, 0);
    if (mutex->prioridad_activa)
    {
      if (mutex->estado == -1)
      {
        mutex->prioridad_siguiente = -1;
      }
      else
      {
        mutex->prioridad_siguiente =
            get_prioridad_pcb(list_get(mutex->lista, 0));
      }
      insertar_prioridad(pcb, mutex->prioridad_siguiente, mutex->logger);
    }
    log_mutex_tomado(mutex->logger, mutex->proceso_actual->pid, mutex->id);
    cambio_desbloquear(pcb, mutex->colas);
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
    cambio_desbloquear(pcb, mutex->colas);
  }
  list_iterator_destroy(iterador_lista);
  free(mutex->id);
  pthread_mutex_unlock(&(mutex->mutex));
  pthread_mutex_destroy(&(mutex->mutex));
  list_destroy(mutex->lista);
  free(mutex);
}

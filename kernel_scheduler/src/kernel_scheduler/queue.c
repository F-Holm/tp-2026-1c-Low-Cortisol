#include "kernel_scheduler/queue.h"

#include "kernel_scheduler/misc.h"

void inicializar_cola(t_cola* cola)
{
  cola->cola = queue_create();
  pthread_mutex_init(&(cola->mutex_cola), NULL);
}

void inicializar_cola_ready(t_cola_ready* colas, int algoritmo,
                            t_list* algoritmos_cmn)
{
  if (algoritmo == AP_CMN)
  {
    colas->cantidad_colas = list_size(algoritmos_cmn);
    colas->colas =
        malloc(colas->cantidad_colas * sizeof(t_cola_individual_ready));
    t_list_iterator* iterator_algoritmos = list_iterator_create(algoritmos_cmn);
    for (int i = 0; i < colas->cantidad_colas; i++)
    {
      colas->colas[i].algoritmo =
          *(int*)list_iterator_next(iterator_algoritmos);
      colas->colas[i].cola = queue_create();
      pthread_mutex_init(&(colas->colas[i].mutex_cola), NULL);
    }
    list_iterator_destroy(iterator_algoritmos);
  }
  else
  {
    colas->cantidad_colas = 1;
    colas->colas = malloc(sizeof(t_cola_individual_ready));
    colas->colas->cola = queue_create();
    pthread_mutex_init(&(colas->colas->mutex_cola), NULL);
  }
}

void inicializar_cola_exec(t_cola_exec* cola, int quantum, bool desalojo)
{
  cola->cola = queue_create();
  pthread_mutex_init(&(cola->mutex_cola), NULL);
  cola->prioridad_mas_baja = NULL;
  cola->quantum = quantum;
  cola->desalojo = desalojo;
}

void inicializar_lista(t_lista* lista)
{
  lista->lista = list_create();
  pthread_mutex_init(&(cola->mutex_lista), NULL);
}

void inicializar_colas(t_colas* colas, int algoritmo, t_list* algoritmos_cmn,
                       int quantum, bool desalojo)
{
  inicializar_cola(colas->new);
  inicializar_cola_ready(colas->ready, algoritmo, algoritmos_cmn);
  inicializar_cola_exec(cola->exec, quantum, desalojo);
  inicializar_cola(colas->block);
  inicializar_lista(colas->susp_block);
  inicializar_lista(colas->susp_ready);
  inicializar_cola(colas->exit);
}

void destruir_cola(t_cola* cola)
{
  queue_destroy(cola->cola);
  pthread_mutex_destroy(&(cola->mutex_cola), NULL);
}

void destruir_cola_ready(t_cola_ready* cola)
{
  for (int i = 0; i < colas->cantidad_colas; i++)
  {
    queue_destroy(cola->colas[i].cola);
    pthread_mutex_destroy(&(cola->colas[i].mutex_cola), NULL);
  }
  free(cola->colas);
}

void destruir_cola_exec(t_cola_exec* cola)
{
  queue_destroy(cola->cola);
  pthread_mutex_destroy(&(cola->mutex_cola), NULL);
}

void destruir_lista(t_lista* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(cola->mutex_lista), NULL);
}

void destruir_colas(t_colas* colas)
{
  destruir_cola(colas->new);
  destruir_cola_ready(colas->ready);
  destruir_cola_exec(cola->exec);
  destruir_cola(colas->block);
  destruir_lista(colas->susp_block);
  destruir_lista(colas->susp_ready);
  destruir_cola(colas->exit);
}

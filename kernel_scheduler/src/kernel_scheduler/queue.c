#include "kernel_scheduler/queue.h"

#include "kernel_scheduler/misc.h"

const char* const ESTADOS_STR[7] = {
    "NEW", "READY", "EXEC", "BLOCK", "SUSP. BLOCK", "SUSP. READY", "EXIT"};

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
    colas->cola_multi_nivel = true;
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
    colas->cola_multi_nivel = false;
    colas->cantidad_colas = 1;
    colas->colas = malloc(sizeof(t_cola_individual_ready));
    colas->colas->cola = queue_create();
    pthread_mutex_init(&(colas->colas->mutex_cola), NULL);
  }
}

void inicializar_lista_exec(t_lista_execute* lista, int quantum, bool desalojo)
{
  lista->lista = list_create();
  pthread_mutex_init(&(lista->mutex_lista), NULL);
  lista->prioridad_mas_baja = NULL;
  lista->quantum = quantum;
  lista->desalojo = desalojo;
}

void inicializar_lista(t_lista* lista)
{
  lista->lista = list_create();
  pthread_mutex_init(&(lista->mutex_lista), NULL);
}

void inicializar_colas(t_colas* colas, int algoritmo, t_list* algoritmos_cmn,
                       int quantum, bool desalojo)
{
  inicializar_cola_ready(&(colas->ready), algoritmo, algoritmos_cmn);
  inicializar_lista_exec(&(colas->exec), quantum, desalojo);
  inicializar_lista(&(colas->block));
  inicializar_lista(&(colas->susp_block));
  inicializar_lista(&(colas->susp_ready));
}

void destruir_cola(t_cola* cola)
{
  queue_destroy(cola->cola);
  pthread_mutex_destroy(&(cola->mutex_cola));
}

void destruir_cola_ready(t_cola_ready* cola)
{
  for (int i = 0; i < cola->cantidad_colas; i++)
  {
    queue_destroy(cola->colas[i].cola);
    pthread_mutex_destroy(&(cola->colas[i].mutex_cola));
  }
  free(cola->colas);
}

void destruir_lista_exec(t_lista_execute* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
}

void destruir_lista(t_lista* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
}

void destruir_colas(t_colas* colas)
{
  destruir_cola_ready(&(colas->ready));
  destruir_lista_exec(&(colas->exec));
  destruir_lista(&(colas->block));
  destruir_lista(&(colas->susp_block));
  destruir_lista(&(colas->susp_ready));
}

void log_cambio_estado(t_logger* logger, uint32_t pid, int estado_anterior,
                       int estado_nuevo)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## %d Pasa del estado %s al estado %s", pid,
           ESTADOS_STR[estado_anterior], ESTADOS_STR[estado_nuevo]);
  pthread_mutex_unlock(&(logger->mutex_logger));
}

bool esta_bloqueado(t_pcb* pcb)
{
  pthread_mutex_lock(&(pcb->mutex_pcb));
  bool ret = pcb->tiempo_bloqueado != 0;
  pthread_mutex_unlock(&(pcb->mutex_pcb));
  return ret;
}

bool puedo_suspender(t_pcb* pcb, int suspension_timeout)
{
  pthread_mutex_lock(&(pcb->mutex_pcb));
  bool ret = time_diff(millis(), pcb->tiempo_bloqueado) >= suspension_timeout;
  pthread_mutex_unlock(&(pcb->mutex_pcb));
  return ret;
}

void set_tiempo_bloqueado(t_pcb* pcb, unsigned long tiempo)
{
  pthread_mutex_lock(&(pcb->mutex_pcb));
  pcb->tiempo_bloqueado = tiempo;
  pthread_mutex_unlock(&(pcb->mutex_pcb));
}

void update_priordad_mas_baja_exec(t_lista_execute* exec)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  exec->prioridad_mas_baja = NULL;
  t_list_iterator* iterator_lista = list_iterator_create(exec->lista);
  while (list_iterator_has_next(iterator_lista))
  {
    t_pcb* pcb = list_iterator_next(iterator_lista);
    if (exec->prioridad_mas_baja == NULL)
    {
      exec->prioridad_mas_baja = pcb;
    }
    else
    {
      pthread_mutex_lock(&(exec->prioridad_mas_baja->mutex_pcb));
      pthread_mutex_lock(&(pcb->mutex_pcb));
      if (exec->prioridad_mas_baja->prioridad > pcb->prioridad)
      {
        exec->prioridad_mas_baja = pcb;
      }
      pthread_mutex_unlock(&(pcb->mutex_pcb));
      pthread_mutex_unlock(&(exec->prioridad_mas_baja->mutex_pcb));
    }
  }
  list_iterator_destroy(iterator_lista);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

void cambio_a_ready(t_pcb* pcb, t_cola_ready* ready, t_logger* logger)
{
  if (ready->cola_multi_nivel)
  {
    pthread_mutex_lock(&(pcb->mutex_pcb));
    int prioridad = pcb->prioridad;
    pthread_mutex_unlock(&(pcb->mutex_pcb));
    if (prioridad <= ready->cantidad_colas)
    {
      pthread_mutex_lock(&(ready->colas[prioridad].mutex_cola));
      queue_push(ready->colas[prioridad].cola, pcb);
      pthread_mutex_unlock(&(ready->colas[prioridad].mutex_cola));
    }
    else
    {
      pthread_mutex_lock(&(pcb->mutex_pcb));
      pthread_mutex_lock(&(logger->mutex_logger));
      log_info(logger->logger, "## Proceso %d con prioridad no válida",
               pcb->pid);
      pthread_mutex_unlock(&(logger->mutex_logger));
      pthread_mutex_unlock(&(pcb->mutex_pcb));
      pthread_mutex_destroy(&(pcb->mutex_pcb));
      free(pcb);
    }
  }
  else
  {
    pthread_mutex_lock(&(ready->colas->mutex_cola));
    queue_push(ready->colas->cola, pcb);
    pthread_mutex_unlock(&(ready->colas->mutex_cola));
  }
}

void cambio_a_exec(t_pcb* pcb, t_lista_execute* exec)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  if (exec->desalojo)
  {
    pthread_mutex_lock(&(exec->prioridad_mas_baja->mutex_pcb));
    pthread_mutex_lock(&(pcb->mutex_pcb));
    if (exec->prioridad_mas_baja->prioridad >= pcb->prioridad)
    {
      exec->prioridad_mas_baja = pcb;
    }
    pthread_mutex_unlock(&(pcb->mutex_pcb));
    pthread_mutex_unlock(&(exec->prioridad_mas_baja->mutex_pcb));
  }
  list_add(exec->lista, pcb);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

void cambio_a_block(t_pcb* pcb, t_lista* block)
{
  set_tiempo_bloqueado(pcb, millis());
  pthread_mutex_lock(&(block->mutex_lista));
  list_add(block->lista, pcb);
  pthread_mutex_unlock(&(block->mutex_lista));
}

void cambio_a_susp_block(t_pcb* pcb, t_lista* susp_block)
{
  pthread_mutex_lock(&(susp_block->mutex_lista));
  list_add(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

void cambio_a_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  list_add(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

void cambio_a_exit(t_pcb* pcb)
{  // Creo que también hay que avisarle a kernel memory
  destruir_pcb(pcb);
}

t_pcb* cambio_sacar_ready(t_cola_ready* ready)
{
  t_pcb* pcb = NULL;
  for (int i = 0; i < ready->cantidad_colas; i++)
  {
    pthread_mutex_lock(&(ready->colas[i].mutex_cola));
    if (!queue_is_empty(ready->colas[i].cola))
    {
      pcb = queue_pop(ready->colas[i].cola);
    }
    pthread_mutex_unlock(&(ready->colas[i].mutex_cola));
    if (pcb != NULL)
      return pcb;
  }
  return NULL;
}

void cambio_sacar_exec(t_pcb* pcb, t_lista_execute* exec)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  list_remove_element(exec->lista, pcb);
  bool actualizar_elemento = pcb == exec->prioridad_mas_baja;
  pthread_mutex_unlock(&(exec->mutex_lista));
  if (actualizar_elemento)
  {
    update_priordad_mas_baja_exec(exec);
  }
}

void cambio_sacar_block(t_pcb* pcb, t_lista* block)
{
  pthread_mutex_lock(&(block->mutex_lista));
  list_remove_element(block->lista, pcb);
  pthread_mutex_unlock(&(block->mutex_lista));
  set_tiempo_bloqueado(pcb, 0);
}

void cambio_sacar_susp_block(t_pcb* pcb, t_lista* susp_block)
{
  pthread_mutex_lock(&(susp_block->mutex_lista));
  list_remove_element(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

void cambio_sacar_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  list_remove_element(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

void cambio_new_ready(t_pcb* pcb, t_cola_ready* ready, t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_NEW, EST_READY);
  cambio_a_ready(pcb, ready, logger);
}

void cambio_ready_exec(t_pcb* pcb, t_lista_execute* exec, t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_READY, EST_EXEC);
}

void cambio_exec_ready(t_pcb* pcb, t_lista_execute* exec, t_cola_ready* ready,
                       t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_EXEC, EST_READY);
  cambio_sacar_exec(pcb, exec);
  cambio_a_ready(pcb, ready, logger);
}

void cambio_exec_exit(t_pcb* pcb, t_lista_execute* exec, t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_EXEC, EST_EXIT);
  cambio_sacar_exec(pcb, exec);
  cambio_a_exit(pcb);
}

void cambio_exec_block(t_pcb* pcb, t_lista_execute* exec, t_lista* block,
                       t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_EXEC, EST_EXIT);
  cambio_sacar_exec(pcb, exec);
  cambio_a_block(pcb, block);
}

void cambio_block_ready(t_pcb* pcb, t_lista* block, t_cola_ready* ready,
                        t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_BLOCK, EST_READY);
  cambio_sacar_block(pcb, block);
  cambio_a_ready(pcb, ready, logger);
}

void cambio_block_susp_block(t_pcb* pcb, t_lista* block, t_lista* susp_block,
                             t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_BLOCK, EST_SUSP_BLOCK);
  cambio_sacar_block(pcb, block);
  cambio_a_susp_block(pcb, susp_block);
}

void cambio_susp_block_block(t_pcb* pcb, t_lista* susp_block, t_lista* block,
                             t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_SUSP_BLOCK, EST_BLOCK);
  cambio_sacar_susp_block(pcb, susp_block);
  cambio_a_block(pcb, block);
}

void cambio_susp_block_susp_ready(t_pcb* pcb, t_lista* susp_block,
                                  t_lista* susp_ready, t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_SUSP_BLOCK, EST_SUSP_READY);
  cambio_sacar_susp_block(pcb, susp_block);
  cambio_a_susp_ready(pcb, susp_ready);
}

void cambio_susp_ready_ready(t_pcb* pcb, t_lista* susp_ready,
                             t_cola_ready* ready, t_logger* logger)
{
  log_cambio_estado(logger, pcb->pid, EST_SUSP_READY, EST_READY);
  cambio_sacar_susp_ready(pcb, susp_ready);
  cambio_a_ready(pcb, ready, logger);
}

void cambio_desbloquear(t_pcb* pcb, t_lista* block, t_lista* susp_block,
                        t_lista* susp_ready, t_cola_ready* ready,
                        t_logger* logger)
{
  if (esta_bloqueado(pcb))
  {
    cambio_block_ready(pcb, block, ready, logger);
  }
  else
  {
    cambio_susp_block_susp_ready(pcb, susp_block, susp_ready, logger);
  }
}

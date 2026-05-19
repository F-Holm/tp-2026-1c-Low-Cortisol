#include "kernel_scheduler/queue.h"

#include "kernel_scheduler/misc.h"

const char* const ESTADOS_STR[7] = {
    "NEW", "READY", "EXEC", "BLOCK", "SUSP. BLOCK", "SUSP. READY", "EXIT"};

const char* const MOTIVOS_FIN_PROCESO[3] = {
    "prioridad no válida", "instrucción EXIT", "cierre del sistema"};

void inicializar_cola(t_cola* cola)
{
  cola->cola = queue_create();
  pthread_mutex_init(&(cola->mutex_cola), NULL);
}

void inicializar_cola_ready(t_cola_ready* cola, int algoritmo,
                            t_list* algoritmos_cmn)
{
  if (algoritmo == AP_CMN)
  {
    cola->cola_multi_nivel = true;
    cola->cantidad_colas = list_size(algoritmos_cmn);
    cola->colas =
        malloc(cola->cantidad_colas * sizeof(t_cola_individual_ready));
    t_list_iterator* iterator_algoritmos = list_iterator_create(algoritmos_cmn);
    for (int i = 0; i < cola->cantidad_colas; i++)
    {
      cola->colas[i].algoritmo = *(int*)list_iterator_next(iterator_algoritmos);
      cola->colas[i].cola = queue_create();
    }
    list_iterator_destroy(iterator_algoritmos);
  }
  else
  {
    cola->cola_multi_nivel = false;
    cola->cantidad_colas = 1;
    cola->colas = malloc(sizeof(t_cola_individual_ready));
    cola->colas->cola = queue_create();
  }
  pthread_mutex_init(&(cola->mutex_cola), NULL);
  pthread_cond_init(&(cola->nuevo_proceso), NULL);
  pthread_mutex_init(&(cola->bloquear_salida), NULL);
  pthread_cond_init(&(cola->salida_desbloqueada), NULL);
  cola->desalojar_todo = false;
  cola->mayor_prioridad = 1000;
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

t_colas* inicializar_colas(int algoritmo, t_list* algoritmos_cmn, int quantum,
                           bool desalojo, int socket_servidor, t_logger* logger)
{
  t_colas* colas = malloc(sizeof(t_colas));
  inicializar_cola_ready(&(colas->ready), algoritmo, algoritmos_cmn);
  inicializar_lista_exec(&(colas->exec), quantum, desalojo);
  inicializar_lista(&(colas->block));
  inicializar_lista(&(colas->susp_block));
  inicializar_lista(&(colas->susp_ready));
  colas->contador_procesos =
      inicializar_contador_procesos(socket_servidor, logger);
  return colas;
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
  }
  pthread_cond_broadcast(&(cola->nuevo_proceso));
  pthread_cond_destroy(&(cola->nuevo_proceso));
  pthread_cond_broadcast(&(cola->salida_desbloqueada));
  pthread_cond_destroy(&(cola->salida_desbloqueada));
  pthread_mutex_destroy(&(cola->mutex_cola));
  pthread_mutex_destroy(&(cola->bloquear_salida));
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
  destruir_contador_procesos(colas->contador_procesos);
  free(colas);
}

bool esta_cola_ready_bloqueada(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->bloquear_salida));
  bool ret = ready->desalojar_todo;
  pthread_mutex_unlock(&(ready->bloquear_salida));
  return ret;
}

void bloquear_cola_ready(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->bloquear_salida));
  ready->desalojar_todo = true;
  pthread_mutex_unlock(&(ready->bloquear_salida));
}

void desbloquear_cola_ready(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->bloquear_salida));
  ready->desalojar_todo = true;
  pthread_mutex_unlock(&(ready->bloquear_salida));
  pthread_cond_broadcast(&(ready->salida_desbloqueada));
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

static bool check_prioridad_valida(t_pcb* pcb, t_cola_ready* ready,
                                   t_logger* logger)
{
  return !ready->cola_multi_nivel ||
         get_prioridad_pcb(pcb) < ready->cantidad_colas;
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
  pthread_mutex_unlock(&(ready->mutex_cola));
  if (ready->cant_procesos_ready == 0)
  {
    pthread_cond_signal(&(ready->nuevo_proceso));
  }

  int prioridad = get_prioridad_pcb(pcb);
  if (ready->cant_procesos_ready == 0 || ready->mayor_prioridad > prioridad)
  {
    ready->mayor_prioridad = get_prioridad_pcb(pcb);
  }

  ready->cant_procesos_ready++;

  if (ready->cola_multi_nivel)
  {
    queue_push(ready->colas[prioridad].cola, pcb);
  }
  else
  {
    queue_push(ready->colas->cola, pcb);
  }
  pthread_mutex_unlock(&(ready->mutex_cola));
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

static void log_cambio_a_exit(t_logger* logger, uint32_t pid, int motivo)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## %u finalizó su ejecución con motivo de %s", pid,
           MOTIVOS_FIN_PROCESO[motivo]);
  pthread_mutex_unlock(&(logger->mutex_logger));
}

void cambio_a_exit(t_pcb* pcb, t_contador_procesos* contador, int motivo,
                   t_logger* logger, t_socket_kernel_memory* socket_km,
                   int socket_servidor)
{  // Creo que también hay que avisarle a kernel memory
  if (motivo == MFP_INSTRUCCION_EXIT)
  {
    if (!avisar_terminar_proceso(socket_km, pcb->pid))
    {
      cerrar_kernel_scheduler(socket_servidor, logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
    }
  }
  log_cambio_a_exit(logger, pcb->pid, motivo);
  destruir_pcb(pcb);
  disminuir_contador_procesos(contador);
}

t_pcb* cambio_sacar_new(char* archivo_instrucciones, int prioridad,
                        t_logger* logger, t_socket_kernel_memory* socket_km,
                        int socket_servidor, t_contador_procesos* contador)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  t_pcb* pcb = crear_pcb();
  log_info(logger->logger, "## %u Se crea el proceso - Estado: NEW", pcb->pid);
  pcb->prioridad = prioridad;
  pthread_mutex_unlock(&(logger->mutex_logger));
  aumentar_contador_procesos(contador);
  if (!avisar_nuevo_proceso(socket_km, archivo_instrucciones, pcb->pid))
  {
    log_cambio_estado(logger, pcb->pid, EST_NEW, EST_EXIT);
    cambio_a_exit(pcb, contador, MFP_CIERRE_SISTEMA, logger, socket_km,
                  socket_servidor);
    cerrar_kernel_scheduler(socket_servidor, logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return NULL;
  }
  return pcb;
}

static t_pcb* cambio_sacar_ready_sin_mutex(t_cola_ready* ready)
{
  for (int i = 0; i < ready->cantidad_colas; i++)
  {
    if (!queue_is_empty(ready->colas[i].cola))
    {
      ready->cant_procesos_ready--;
      t_pcb* pcb = queue_pop(ready->colas[i].cola);
      if (queue_is_empty(ready->colas[i].cola) && ready->cola_multi_nivel &&
          ready->mayor_prioridad == i && ready->cant_procesos_ready != 0)
      {
        for (++i; i < ready->cantidad_colas; i++)
        {
          if (!queue_is_empty(ready->colas[i].cola))
          {
            ready->mayor_prioridad = i;
            break;
          }
        }
      }
      return pcb;
    }
  }
  return NULL;
}

t_pcb* cambio_sacar_ready(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  t_pcb* pcb = cambio_sacar_ready_sin_mutex(ready);
  pthread_mutex_unlock(&(ready->mutex_cola));
  return pcb;
}

t_pcb* cambio_sacar_ready_bloqueante(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  while (ready->cant_procesos_ready == 0)
  {
    pthread_cond_wait(&(ready->nuevo_proceso), &(ready->mutex_cola));
  }
  pthread_mutex_lock(&(ready->bloquear_salida));
  while (ready->desalojar_todo)
  {
    pthread_cond_wait(&(ready->salida_desbloqueada), &(ready->bloquear_salida));
  }
  pthread_mutex_unlock(&(ready->bloquear_salida));
  t_pcb* pcb = cambio_sacar_ready_sin_mutex(ready);
  pthread_mutex_unlock(&(ready->mutex_cola));
  return pcb;
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

t_pcb* cambio_sacar_exec_siguiente(t_lista_execute* exec)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(exec->mutex_lista));
  if (!list_is_empty(exec->lista))
  {
    pcb = list_remove(exec->lista, 0);
  }
  pthread_mutex_unlock(&(exec->mutex_lista));
  return pcb;
}

void cambio_sacar_block(t_pcb* pcb, t_lista* block)
{
  pthread_mutex_lock(&(block->mutex_lista));
  list_remove_element(block->lista, pcb);
  pthread_mutex_unlock(&(block->mutex_lista));
  set_tiempo_bloqueado(pcb, 0);
}

t_pcb* cambio_sacar_block_siguiente(t_lista* block)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(block->mutex_lista));
  if (!list_is_empty(block->lista))
  {
    pcb = list_remove(block->lista, 0);
  }
  pthread_mutex_unlock(&(block->mutex_lista));
  if (pcb != NULL)
  {
    set_tiempo_bloqueado(pcb, 0);
  }
  return pcb;
}

void cambio_sacar_susp_block(t_pcb* pcb, t_lista* susp_block)
{
  pthread_mutex_lock(&(susp_block->mutex_lista));
  list_remove_element(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

t_pcb* cambio_sacar_susp_block_siguiente(t_lista* susp_block)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(susp_block->mutex_lista));
  if (!list_is_empty(susp_block->lista))
  {
    pcb = list_remove(susp_block->lista, 0);
  }
  pthread_mutex_unlock(&(susp_block->mutex_lista));
  return pcb;
}

void cambio_sacar_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  list_remove_element(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

t_pcb* cambio_sacar_susp_ready_siguiente(t_lista* susp_ready)
{
  t_pcb* pcb = NULL;
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  if (!list_is_empty(susp_ready->lista))
  {
    pcb = list_remove(susp_ready->lista, 0);
  }
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
  return pcb;
}

void cambio_new_ready(t_pcb* pcb, t_cola_ready* ready, t_logger* logger,
                      t_contador_procesos* contador,
                      t_socket_kernel_memory* socket_km, int socket_servidor)
{
  if (!check_prioridad_valida(pcb, ready, logger))
  {
    log_cambio_estado(logger, pcb->pid, EST_NEW, EST_EXIT);
    cambio_a_exit(pcb, contador, MFP_PRIORIDAD_NO_VALIDA, logger, socket_km,
                  socket_servidor);
  }
  else
  {
    log_cambio_estado(logger, pcb->pid, EST_NEW, EST_READY);
    cambio_a_ready(pcb, ready, logger);
  }
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

void cambio_exec_exit(t_pcb* pcb, t_lista_execute* exec, t_logger* logger,
                      t_contador_procesos* contador,
                      t_socket_kernel_memory* socket_km, int socket_servidor)
{
  log_cambio_estado(logger, pcb->pid, EST_EXEC, EST_EXIT);
  cambio_sacar_exec(pcb, exec);
  cambio_a_exit(pcb, contador, MFP_INSTRUCCION_EXIT, logger, socket_km,
                socket_servidor);
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

bool cambio_cualquiera_exit(t_colas* colas, t_logger* logger, int estado,
                            t_contador_procesos* contador,
                            t_socket_kernel_memory* socket_km,
                            int socket_servidor)
{
  t_pcb* pcb = NULL;
  switch (estado)
  {
    case EST_READY:
      pcb = cambio_sacar_ready(&(colas->ready));
      break;
    case EST_EXEC:
      pcb = cambio_sacar_exec_siguiente(&(colas->exec));
      break;
    case EST_BLOCK:
      pcb = cambio_sacar_block_siguiente(&(colas->block));
      break;
    case EST_SUSP_BLOCK:
      pcb = cambio_sacar_susp_block_siguiente(&(colas->susp_block));
      break;
    case EST_SUSP_READY:
      pcb = cambio_sacar_susp_ready_siguiente(&(colas->susp_ready));
      break;
  }
  if (pcb == NULL)
  {
    return false;
  }
  log_cambio_estado(logger, pcb->pid, estado, EST_EXIT);
  cambio_a_exit(pcb, contador, MFP_CIERRE_SISTEMA, logger, socket_km,
                socket_servidor);
  return true;
}

void vaciar_colas(t_colas* colas, t_logger* logger,
                  t_socket_kernel_memory* socket_km, int socket_servidor)
{
  for (int i = EST_READY; i < EST_EXIT; i++)
  {
    while (cambio_cualquiera_exit(colas, logger, i, colas->contador_procesos,
                                  socket_km, socket_servidor))
      ;
  }
}

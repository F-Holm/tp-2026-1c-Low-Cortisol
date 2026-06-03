#include "kernel_scheduler/queue.h"

#include <unistd.h>

#include "kernel_scheduler/misc.h"

const char* const MOTIVOS_FIN_PROCESO[4] = {
    "prioridad no válida", "instrucción EXIT", "cierre del sistema",
    "fallo de io"};

static void* hilo_suspensor(void* datos_void);
static void* hilo_des_suspensor(void* datos_void);

static void inicializar_cola_ready(t_cola_ready* cola, int algoritmo,
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
  pthread_mutex_init(&(cola->mutex_desalojo_prioritario), NULL);
  cola->desalojar_todo = false;
  cola->mayor_prioridad = 1000;
}

static void inicializar_lista_exec(t_lista_execute* lista, int quantum,
                                   bool desalojo)
{
  lista->lista = list_create();
  pthread_mutex_init(&(lista->mutex_lista), NULL);
  lista->prioridad_mas_baja = NULL;
  lista->quantum = quantum;
  lista->desalojo = desalojo;
}

static void inicializar_lista(t_lista* lista)
{
  lista->lista = list_create();
  pthread_mutex_init(&(lista->mutex_lista), NULL);
  pthread_cond_init(&(lista->cond_nuevo_proceso), NULL);
}

static t_datos_hilo_suspendido* inicializar_datos_hilo_suspendido(
    t_colas* colas)
{
  t_datos_hilo_suspendido* datos = malloc(sizeof(t_datos_hilo_suspendido));
  pthread_mutex_init(&(datos->mutex_estado), NULL);
  datos->estado = EH_EJECUTANDO;
  datos->mutex_suspender_des_suspender =
      &(colas->mutex_suspender_des_suspender);
  pthread_cond_init(&(datos->desbloquear), NULL);
  return datos;
}

static void inicializar_datos_hilo_suspensor(t_colas* colas,
                                             int suspension_timeout)
{
  t_datos_hilo_suspensor* datos = malloc(sizeof(t_datos_hilo_suspensor));
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  datos->datos->esperar_proceso = &(colas->block.cond_nuevo_proceso);
  datos->suspension_timeout = suspension_timeout;
  colas->datos_suspendido->datos_hilo_suspensor = datos;
}

static void inicializar_datos_hilo_des_suspensor(t_colas* colas)
{
  t_datos_hilo_des_suspensor* datos =
      malloc(sizeof(t_datos_hilo_des_suspensor));
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  datos->datos->esperar_proceso = &(colas->susp_ready.cond_nuevo_proceso);
  colas->datos_suspendido->datos_hilo_des_suspensor = datos;
}

static void iniciar_hilo_suspensor(t_colas* colas)
{
  if (pthread_create(
          &(colas->datos_suspendido->datos_hilo_des_suspensor->datos->hilo),
          NULL, hilo_suspensor, colas) != 0)
  {
    logger_error(colas->logger, "## Error en la creación del hilo suspensor");
  }
  else
  {
    logger_info(colas->logger, "## Hilo suspensor iniciado exitosamente");
  }
}

static void iniciar_hilo_des_suspensor(t_colas* colas)
{
  if (pthread_create(
          &(colas->datos_suspendido->datos_hilo_des_suspensor->datos->hilo),
          NULL, hilo_des_suspensor, colas) != 0)
  {
    logger_error(colas->logger,
                 "## Error en la creación del hilo des-suspensor");
  }
  else
  {
    logger_info(colas->logger, "## Hilo des-suspensor iniciado exitosamente");
  }
}

static void iniciar_hilos_suspendido(t_colas* colas, int suspension_timeout)
{
  colas->datos_suspendido = malloc(sizeof(t_datos_suspendido));
  inicializar_datos_hilo_suspensor(colas, suspension_timeout);
  inicializar_datos_hilo_des_suspensor(colas);
  iniciar_hilo_suspensor(colas);
  iniciar_hilo_des_suspensor(colas);
}

t_colas* inicializar_colas(int algoritmo, t_list* algoritmos_cmn, int quantum,
                           bool desalojo, int socket_servidor, t_logger* logger,
                           t_socket_kernel_memory* socket_km,
                           int suspension_timeout)
{
  t_colas* colas = malloc(sizeof(t_colas));
  inicializar_cola_ready(&(colas->ready), algoritmo, algoritmos_cmn);
  inicializar_lista_exec(&(colas->exec), quantum, desalojo);
  inicializar_lista(&(colas->block));
  inicializar_lista(&(colas->susp_block));
  inicializar_lista(&(colas->susp_ready));
  colas->contador_procesos =
      inicializar_contador_procesos(socket_servidor, logger);
  pthread_mutex_init(&(colas->mutex_suspender_des_suspender), NULL);
  colas->logger = logger;
  colas->socket_km = socket_km;
  colas->socket_servidor = socket_servidor;
  iniciar_hilos_suspendido(colas, suspension_timeout);
  return colas;
}

static void destruir_cola_ready(t_cola_ready* cola)
{
  for (int i = 0; i < cola->cantidad_colas; i++)
  {
    queue_destroy(cola->colas[i].cola);
  }
  pthread_cond_destroy(&(cola->nuevo_proceso));
  pthread_cond_destroy(&(cola->salida_desbloqueada));
  pthread_mutex_destroy(&(cola->mutex_cola));
  pthread_mutex_destroy(&(cola->bloquear_salida));
  pthread_mutex_destroy(&(cola->mutex_desalojo_prioritario));
  free(cola->colas);
}

static void destruir_lista_exec(t_lista_execute* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
}

static void destruir_lista(t_lista* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
  pthread_cond_destroy(&(lista->cond_nuevo_proceso));
}

static void terminar_hilo_suspendido(t_datos_hilo_suspendido* datos)
{
  pthread_mutex_lock(&(datos->mutex_estado));
  pthread_cond_signal(datos->esperar_proceso);
  pthread_cond_signal(&(datos->desbloquear));
  datos->estado = EH_FINALIZANDO;
  pthread_mutex_unlock(&(datos->mutex_estado));
}

static void esperar_hilo_suspendido(t_datos_hilo_suspendido* datos)
{
  pthread_join(datos->hilo, NULL);
  datos->estado = EH_FINALIZADO;
}

static void destruir_hilo_suspendido(t_datos_hilo_suspendido* datos)
{
  pthread_mutex_destroy(&(datos->mutex_estado));
  pthread_cond_destroy(&(datos->desbloquear));
  free(datos);
}

static void destruir_hilo_suspensor(t_datos_hilo_suspensor* datos)
{
  destruir_hilo_suspendido(datos->datos);
  free(datos);
}

static void destruir_hilo_des_suspensor(t_datos_hilo_des_suspensor* datos)
{
  destruir_hilo_suspendido(datos->datos);
  free(datos);
}

void terminar_hilos_suspendido(t_colas* colas)
{
  terminar_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_suspensor->datos);
  terminar_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
  esperar_hilo_suspendido(colas->datos_suspendido->datos_hilo_suspensor->datos);
  esperar_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
}

void destruir_hilos_suspendido(t_colas* colas)
{
  destruir_hilo_suspensor(colas->datos_suspendido->datos_hilo_suspensor);
  destruir_hilo_des_suspensor(
      colas->datos_suspendido->datos_hilo_des_suspensor);
  free(colas->datos_suspendido);
}

void destruir_colas(t_colas* colas)
{
  terminar_hilos_suspendido(colas);
  destruir_hilos_suspendido(colas);
  destruir_cola_ready(&(colas->ready));
  destruir_lista_exec(&(colas->exec));
  destruir_lista(&(colas->block));
  destruir_lista(&(colas->susp_block));
  destruir_lista(&(colas->susp_ready));
  destruir_contador_procesos(colas->contador_procesos);
  pthread_mutex_destroy(&(colas->mutex_suspender_des_suspender));
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

static void log_cambio_estado(t_logger* logger, uint32_t pid,
                              int estado_anterior, int estado_nuevo)
{
  logger_info(logger, "## %d Pasa del estado %s al estado %s", pid,
              ESTADOS_STR[estado_anterior], ESTADOS_STR[estado_nuevo]);
}

static void log_estado_no_valido(t_logger* logger, uint32_t pid, int estado,
                                 int estado_esperado, int estado_futuro)
{
  logger_error(logger,
               "## %d No puede pasar del estado %s al estado %s porque se "
               "encuentra en el estado %s",
               pid, ESTADOS_STR[estado_esperado], ESTADOS_STR[estado_futuro],
               ESTADOS_STR[estado]);
}

static bool gestionar_estado_pcb(t_logger* logger, t_pcb* pcb,
                                 int estado_esperado, int estado_futuro)
{
  if (pcb->estado == estado_esperado)
  {
    log_cambio_estado(logger, pcb->pid, estado_esperado, estado_futuro);
    pcb->estado = estado_futuro;
    return true;
  }
  log_estado_no_valido(logger, pcb->pid, pcb->estado, estado_esperado,
                       estado_futuro);
  return false;
}

static bool esta_bloqueado(t_pcb* pcb)
{
  return pcb->estado == EST_BLOCK;
}

bool puedo_suspender(t_pcb* pcb, int suspension_timeout)
{
  return time_diff(millis(), pcb->tiempo_bloqueado) >= suspension_timeout;
}

static void set_tiempo_bloqueado(t_pcb* pcb, unsigned long tiempo)
{
  pcb->tiempo_bloqueado = tiempo;
}

static bool check_prioridad_valida(t_pcb* pcb, t_cola_ready* ready,
                                   t_logger* logger)
{
  return !ready->cola_multi_nivel ||
         get_prioridad_pcb(pcb) < ready->cantidad_colas;
}

static void update_priordad_mas_baja_exec(t_lista_execute* exec)
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
      pthread_mutex_lock(&(exec->prioridad_mas_baja->mutex_prioridad));
      pthread_mutex_lock(&(pcb->mutex_prioridad));
      if (exec->prioridad_mas_baja->prioridad > pcb->prioridad)
      {
        exec->prioridad_mas_baja = pcb;
      }
      pthread_mutex_unlock(&(pcb->mutex_prioridad));
      pthread_mutex_unlock(&(exec->prioridad_mas_baja->mutex_prioridad));
    }
  }
  list_iterator_destroy(iterator_lista);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

static void cambio_a_ready(t_pcb* pcb, t_cola_ready* ready, t_logger* logger)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  int prioridad = get_prioridad_pcb(pcb);
  if (ready->cant_procesos_ready == 0 || ready->mayor_prioridad > prioridad)
  {
    ready->mayor_prioridad = get_prioridad_pcb(pcb);
  }

  if (ready->cant_procesos_ready == 0)
  {
    pthread_cond_signal(&(ready->nuevo_proceso));
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
    pthread_mutex_lock(&(exec->prioridad_mas_baja->mutex_prioridad));
    pthread_mutex_lock(&(pcb->mutex_prioridad));
    if (exec->prioridad_mas_baja->prioridad >= pcb->prioridad)
    {
      exec->prioridad_mas_baja = pcb;
    }
    pthread_mutex_unlock(&(pcb->mutex_prioridad));
    pthread_mutex_unlock(&(exec->prioridad_mas_baja->mutex_prioridad));
  }
  list_add(exec->lista, pcb);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

static void cambio_a_block(t_pcb* pcb, t_lista* block)
{
  set_tiempo_bloqueado(pcb, millis());
  pthread_mutex_lock(&(block->mutex_lista));
  if (list_size(block->lista) == 0)
  {
    pthread_cond_signal(&(block->cond_nuevo_proceso));
  }
  list_add(block->lista, pcb);
  pthread_mutex_unlock(&(block->mutex_lista));
}

static void cambio_a_susp_block(t_pcb* pcb, t_lista* susp_block)
{
  pthread_mutex_lock(&(susp_block->mutex_lista));
  list_add(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

static void cambio_a_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  if (list_size(susp_ready->lista) == 0)
  {
    pthread_cond_signal(&(susp_ready->cond_nuevo_proceso));
  }
  list_add(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

static void log_cambio_a_exit(t_logger* logger, uint32_t pid, int motivo)
{
  logger_info(logger, "## %u finalizó su ejecución con motivo de %s", pid,
              MOTIVOS_FIN_PROCESO[motivo]);
}

static void cambio_a_exit(t_pcb* pcb, t_contador_procesos* contador, int motivo,
                          t_logger* logger, t_socket_kernel_memory* socket_km,
                          int socket_servidor)
{
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

static t_pcb* cambio_sacar_new(char* archivo_instrucciones, int prioridad,
                               t_logger* logger,
                               t_socket_kernel_memory* socket_km,
                               int socket_servidor,
                               t_contador_procesos* contador)
{
  t_pcb* pcb = crear_pcb();
  logger_info(logger, "## %u Se crea el proceso - Estado: NEW", pcb->pid);
  pcb->prioridad = prioridad;
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

static void actualizar_mayor_prioridad_ready_sin_mutex(t_cola_ready* ready,
                                                       int index)
{
  if (ready->cant_procesos_ready > 0 && ready->cola_multi_nivel)
  {
    while (index < ready->cantidad_colas)
    {
      if (!queue_is_empty(ready->colas[index].cola))
      {
        ready->mayor_prioridad = index;
        return;
      }
    }
  }
  ready->mayor_prioridad = ready->cantidad_colas;
}

static t_pcb* cambio_sacar_ready_sin_mutex(t_cola_ready* ready)
{
  for (int i = 0; i < ready->cantidad_colas; i++)
  {
    if (!queue_is_empty(ready->colas[i].cola))
    {
      ready->cant_procesos_ready--;
      t_pcb* pcb = queue_pop(ready->colas[i].cola);
      if (queue_is_empty(ready->colas[i].cola))
      {
        actualizar_mayor_prioridad_ready_sin_mutex(ready, ++i);
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

static void cambio_sacar_exec(t_pcb* pcb, t_lista_execute* exec)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  list_remove_element(exec->lista, pcb);
  bool actualizar_elemento = pcb == exec->prioridad_mas_baja;
  pthread_mutex_unlock(&(exec->mutex_lista));
  if (exec->desalojo && actualizar_elemento)
  {
    update_priordad_mas_baja_exec(exec);
  }
}

static t_pcb* cambio_sacar_exec_siguiente(t_lista_execute* exec)
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

static void cambio_sacar_block(t_pcb* pcb, t_lista* block)
{
  pthread_mutex_lock(&(block->mutex_lista));
  list_remove_element(block->lista, pcb);
  pthread_mutex_unlock(&(block->mutex_lista));
  set_tiempo_bloqueado(pcb, 0);
}

static t_pcb* cambio_sacar_block_siguiente(t_lista* block)
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

static void cambio_sacar_susp_block(t_pcb* pcb, t_lista* susp_block)
{
  pthread_mutex_lock(&(susp_block->mutex_lista));
  list_remove_element(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

static t_pcb* cambio_sacar_susp_block_siguiente(t_lista* susp_block)
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

static void cambio_sacar_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  list_remove_element(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

static t_pcb* cambio_sacar_susp_ready_siguiente(t_lista* susp_ready)
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

void cambio_new_ready(t_colas* colas, char* archivo_instrucciones,
                      int prioridad)
{
  t_pcb* pcb = cambio_sacar_new(archivo_instrucciones, prioridad, colas->logger,
                                colas->socket_km, colas->socket_servidor,
                                colas->contador_procesos);
  if (pcb != NULL)
  {
    if (!check_prioridad_valida(pcb, &(colas->ready), colas->logger))
    {
      log_cambio_estado(colas->logger, pcb->pid, EST_NEW, EST_EXIT);
      cambio_a_exit(pcb, colas->contador_procesos, MFP_PRIORIDAD_NO_VALIDA,
                    colas->logger, colas->socket_km, colas->socket_servidor);
    }
    else
    {
      log_cambio_estado(colas->logger, pcb->pid, EST_NEW, EST_READY);
      cambio_a_ready(pcb, &(colas->ready), colas->logger);
    }
  }
}

void cambio_ready_exec(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  gestionar_estado_pcb(colas->logger, pcb, EST_READY, EST_EXEC);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_exec_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_READY))
  {
    cambio_sacar_exec(pcb, &(colas->exec));
    cambio_a_ready(pcb, &(colas->ready), colas->logger);
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_exec_exit(t_pcb* pcb, t_colas* colas, int motivo)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_EXIT))
  {
    cambio_sacar_exec(pcb, &(colas->exec));
    cambio_a_exit(pcb, colas->contador_procesos, motivo, colas->logger,
                  colas->socket_km, colas->socket_servidor);
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_exec_block(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_BLOCK))
  {
    cambio_sacar_exec(pcb, &(colas->exec));
    cambio_a_block(pcb, &(colas->block));
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

static void cambio_block_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_BLOCK, EST_READY))
  {
    cambio_sacar_block(pcb, &(colas->block));
    cambio_a_ready(pcb, &(colas->ready), colas->logger);
  }
}

void cambio_block_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_block_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

static void cambio_block_susp_block_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_BLOCK, EST_SUSP_BLOCK))
  {
    cambio_sacar_block(pcb, &(colas->block));
    cambio_a_susp_block(pcb, &(colas->susp_block));
  }
}

void cambio_block_susp_block(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_block_susp_block_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_susp_block_block(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_SUSP_BLOCK, EST_BLOCK))
  {
    cambio_sacar_susp_block(pcb, &(colas->susp_block));
    cambio_a_block(pcb, &(colas->block));
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

static void cambio_susp_block_susp_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_SUSP_BLOCK, EST_SUSP_READY))
  {
    cambio_sacar_susp_block(pcb, &(colas->susp_block));
    cambio_a_susp_ready(pcb, &(colas->susp_ready));
  }
}

void cambio_susp_block_susp_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_susp_block_susp_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

static void cambio_susp_ready_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_SUSP_READY, EST_READY))
  {
    cambio_sacar_susp_ready(pcb, &(colas->susp_ready));
    cambio_a_ready(pcb, &(colas->ready), colas->logger);
  }
}

void cambio_susp_ready_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_susp_ready_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_desbloquear(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (esta_bloqueado(pcb))
  {
    cambio_block_ready_sin_mutex(pcb, colas);
  }
  else
  {
    cambio_susp_block_susp_ready_sin_mutex(pcb, colas);
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

static bool cambio_cualquiera_exit(t_colas* colas, int estado, int motivo)
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

  log_cambio_estado(colas->logger, pcb->pid, estado, EST_EXIT);
  cambio_a_exit(pcb, colas->contador_procesos, motivo, colas->logger,
                colas->socket_km, colas->socket_servidor);
  return true;
}

void vaciar_colas(t_colas* colas)
{
  for (int i = EST_READY; i < EST_EXIT; i++)
  {
    while (cambio_cualquiera_exit(colas, i, MFP_CIERRE_SISTEMA))
    {
    }
  }
}

static void bloquear_hilo_suspendido(t_datos_hilo_suspendido* datos)
{
  pthread_mutex_lock(&(datos->mutex_estado));
  switch (datos->estado)
  {
    case EH_EJECUTANDO:
      datos->estado = EH_BLOQUEADO;
      break;
    case EH_ESPERANDO_PROCESO:
      pthread_cond_signal(datos->esperar_proceso);
      datos->estado = EH_BLOQUEADO;
      break;
  }
  pthread_mutex_unlock(&(datos->mutex_estado));
}

static void desbloquear_hilo_suspendido(t_datos_hilo_suspendido* datos)
{
  pthread_mutex_lock(&(datos->mutex_estado));
  if (datos->estado == EH_BLOQUEADO)
  {
    pthread_cond_signal(&(datos->desbloquear));
    datos->estado = EH_EJECUTANDO;
  }
  pthread_mutex_unlock(&(datos->mutex_estado));
}

void bloquear_hilos_suspendido(t_colas* colas)
{
  bloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_suspensor->datos);
  bloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
}

void desbloquear_hilos_suspendido(t_colas* colas)
{
  desbloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_suspensor->datos);
  desbloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
}

static void esperar_desbloqueo(t_datos_hilo_suspendido* datos)
{
  pthread_cond_wait(datos->esperar_proceso, &(datos->mutex_estado));
  if (datos->estado == EH_BLOQUEADO)
  {
    datos->estado = EH_EJECUTANDO;
  }
}

static t_pcb* obtener_proceso_bloqueado(t_colas* colas,
                                        t_datos_hilo_suspensor* datos)
{
  pthread_mutex_lock(&(colas->block.mutex_lista));
  t_pcb* proceso = NULL;
  if (!list_is_empty(colas->block.lista))
  {
    proceso = list_get(colas->block.lista, 0);
  }
  pthread_mutex_unlock(&(colas->block.mutex_lista));
  if (proceso == NULL)
  {
    datos->datos->estado = EH_ESPERANDO_PROCESO;
  }
  return proceso;
}

static void suspender_proceso(t_colas* colas, t_datos_hilo_suspensor* datos,
                              t_pcb* proceso)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(proceso->mutex_estado));
  unsigned long tiempo_sleep = millis() - proceso->tiempo_bloqueado;

  if (tiempo_sleep >= datos->suspension_timeout)
  {
    if (proceso->estado == EST_BLOCK)
    {
      cambio_block_susp_block_sin_mutex(proceso, colas);
    }
    pthread_mutex_unlock(&(proceso->mutex_estado));
  }
  else
  {
    pthread_mutex_unlock(&(proceso->mutex_estado));
    usleep(tiempo_sleep * 1000);
  }

  pthread_mutex_lock(&(datos->datos->mutex_estado));
}

static void esperar_proceso_bloqueado(t_colas* colas,
                                      t_datos_hilo_suspensor* datos)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(colas->block.mutex_lista));
  if (list_is_empty(colas->block.lista))
  {
    pthread_cond_wait(datos->datos->esperar_proceso,
                      &(datos->datos->mutex_estado));
  }
  bool lista_vacia = list_is_empty(colas->block.lista);
  pthread_mutex_unlock(&(colas->block.mutex_lista));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
  if (!lista_vacia && datos->datos->estado == EH_ESPERANDO_PROCESO)
  {
    datos->datos->estado = EH_EJECUTANDO;
  }
}

static t_pcb* obtener_proceso_susp_ready(t_colas* colas,
                                         t_datos_hilo_des_suspensor* datos)
{
  pthread_mutex_lock(&(colas->susp_ready.mutex_lista));
  t_pcb* proceso = NULL;
  if (!list_is_empty(colas->susp_ready.lista))
  {
    proceso = list_get(colas->susp_ready.lista, 0);
  }
  pthread_mutex_unlock(&(colas->susp_ready.mutex_lista));
  if (proceso == NULL)
  {
    datos->datos->estado = EH_ESPERANDO_PROCESO;
  }
  return proceso;
}

static void des_suspender_proceso(t_colas* colas,
                                  t_datos_hilo_des_suspensor* datos,
                                  t_pcb* proceso)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(proceso->mutex_estado));

  if (proceso->estado == EST_BLOCK)
  {
    cambio_susp_ready_ready_sin_mutex(proceso, colas);
  }

  pthread_mutex_unlock(&(proceso->mutex_estado));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
}

static void esperar_proceso_susp_ready(t_colas* colas,
                                       t_datos_hilo_des_suspensor* datos)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(colas->susp_block.mutex_lista));
  if (list_is_empty(colas->susp_block.lista))
  {
    pthread_cond_wait(datos->datos->esperar_proceso,
                      &(datos->datos->mutex_estado));
  }
  bool lista_vacia = list_is_empty(colas->susp_block.lista);
  pthread_mutex_unlock(&(colas->susp_block.mutex_lista));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
  if (!lista_vacia && datos->datos->estado == EH_ESPERANDO_PROCESO)
  {
    datos->datos->estado = EH_EJECUTANDO;
  }
}

static void* hilo_suspensor(void* datos_void)
{
  t_colas* colas = (t_colas*)datos_void;
  t_datos_hilo_suspensor* datos = colas->datos_suspendido->datos_hilo_suspensor;
  bool seguir_operando = true;

  while (seguir_operando)
  {
    pthread_mutex_lock(&(datos->datos->mutex_estado));
    switch (datos->datos->estado)
    {
      case EH_EJECUTANDO:
        t_pcb* proceso = obtener_proceso_bloqueado(colas, datos);
        if (proceso != NULL)
        {
          suspender_proceso(colas, datos, proceso);
        }
        break;
      case EH_ESPERANDO_PROCESO:
        esperar_proceso_bloqueado(colas, datos);
        break;
      case EH_BLOQUEADO:
        esperar_desbloqueo(datos->datos);
        break;
      case EH_FINALIZANDO:
        seguir_operando = false;
        break;
    }
    pthread_mutex_unlock(&(datos->datos->mutex_estado));
  }
  return NULL;
}

static void* hilo_des_suspensor(void* datos_void)
{
  t_colas* colas = (t_colas*)datos_void;
  t_datos_hilo_des_suspensor* datos =
      colas->datos_suspendido->datos_hilo_des_suspensor;
  bool seguir_operando = true;

  while (seguir_operando)
  {
    pthread_mutex_lock(&(datos->datos->mutex_estado));
    switch (datos->datos->estado)
    {
      case EH_EJECUTANDO:
        t_pcb* proceso = obtener_proceso_susp_ready(colas, datos);
        if (proceso != NULL)
        {
          des_suspender_proceso(colas, datos, proceso);
        }
        break;
      case EH_ESPERANDO_PROCESO:
        esperar_proceso_susp_ready(colas, datos);
        break;
      case EH_BLOQUEADO:
        esperar_desbloqueo(datos->datos);
        break;
      case EH_FINALIZANDO:
        seguir_operando = false;
        break;
    }
    pthread_mutex_unlock(&(datos->datos->mutex_estado));
  }
  return NULL;
}

/*
suspender_proceso

des_suspender_proceso
- ok
- compactación

des_suspender_proceso_sin_compactacion
- ok
- no

rutina de bloqueo total:
- bloquear cola ready
- bloquear hilos de cambios de estado
- bloquear mutex de suspender y des-suspender
- esperar cola ready vacia

rutina de desbloqueo total:
- desbloquear mutex de suspender y des-suspender
- desbloquear hilos de cambios de estado
- desbloquear cola ready

rutina de des-suspensión:
- obtener los 2 primeros elementos de cada lista de suspendido
- des_suspender_proceso_sin_compactacion el más prioritario
- si responde OK: volver a obtener los 2 primeros elementos

rutina de nuevo_stick o memoria liberada:
- rutina de bloqueo total
- rutina de des-suspensión
- rutina de desbloqueo total

rutina compactación:
- suspendo proceso o pido memoria
- recibo compactación
- rutina de bloqueo total
- aviso a KM
- espero confirmación de KM
- rutina de des-suspensión
- rutina de desbloqueo total

Las rutinas crean un hilo de ejecución aparte

Cuando se termina un proceso, si recibe una respuesta si se liberó memoria o no
*/

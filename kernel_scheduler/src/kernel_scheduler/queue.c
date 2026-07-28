#include "kernel_scheduler/queue.h"

#include <limits.h>
#include <unistd.h>

#include "utils/msg.h"

const char* const MOTIVOS_FIN_PROCESO[9] = {
    "prioridad no válida",
    "instrucción EXIT",
    "cierre del sistema",
    "fallo de io",
    "no hay suficiente memoria disponible",
    "segmentation fault",
    "ya existe un mutex con ese nombre",
    "no existe un mutex con ese nombre",
    "este proceso no puede desbloquear este mutex"};

static t_contador* crear_contador(void);
static void sumar_contador(t_contador* contador);
static void sumar_contador_hilos(t_colas* colas);
static void restar_contador_hilos(t_colas* colas);
static void esperar_contador_hilos(t_colas* colas);
static void destruir_contador(t_contador* contador);
static void destruir_contador_hilos(t_colas* colas);
static void destruir_contador_syscalls(t_colas* colas);
static void inicializar_cola_ready(t_cola_ready* cola, int algoritmo,
                                   t_list* algoritmos_cmn);
static void inicializar_lista_exec(t_lista_execute* lista, int quantum,
                                   bool desalojo);
static void inicializar_lista(t_lista* lista);
static t_datos_hilo_suspendido* inicializar_datos_hilo_suspendido(
    t_colas* colas);
static void inicializar_datos_hilo_suspensor(t_colas* colas,
                                             int suspension_timeout);
static void inicializar_datos_hilo_des_suspensor(t_colas* colas);
static void iniciar_hilo_suspensor(t_colas* colas);
static void iniciar_hilo_des_suspensor(t_colas* colas);
static void iniciar_hilos_suspendido(t_colas* colas, int suspension_timeout);
static void destruir_cola_ready(t_cola_ready* cola);
static void destruir_lista_exec(t_lista_execute* lista);
static void destruir_lista(t_lista* lista);
static void terminar_hilo_suspendido(t_datos_hilo_suspendido* datos,
                                     t_lista* lista);
static void esperar_hilo_suspendido(t_datos_hilo_suspendido* datos);
static void destruir_hilo_suspendido(t_datos_hilo_suspendido* datos);
static void destruir_hilo_suspensor(t_datos_hilo_suspensor* datos);
static void destruir_hilo_des_suspensor(t_datos_hilo_des_suspensor* datos);
static void terminar_hilos_suspendido(t_colas* colas);
static void destruir_hilos_suspendido(t_colas* colas);
static void terminar_rutinas(t_colas* colas);
static void log_cambio_estado(t_logger* logger, uint32_t pid,
                              int estado_anterior, int estado_nuevo);
static void log_estado_no_valido(t_logger* logger, uint32_t pid, int estado,
                                 int estado_esperado, int estado_futuro);
static bool gestionar_estado_pcb(t_logger* logger, t_pcb* pcb,
                                 int estado_esperado, int estado_futuro);
static void set_tiempo_bloqueado(t_pcb* pcb, unsigned long tiempo);
static bool check_prioridad_valida(t_pcb* pcb, t_cola_ready* ready,
                                   t_logger* logger);
static void update_priordad_mas_baja_exec(t_lista_execute* exec);
static void cambio_a_ready(t_pcb* pcb, t_cola_ready* ready);
static void cambio_a_block(t_pcb* pcb, t_lista* block);
static void cambio_a_susp_block(t_pcb* pcb, t_lista* susp_block);
static void cambio_a_susp_ready(t_pcb* pcb, t_lista* susp_ready);
static void log_cambio_a_exit(t_logger* logger, uint32_t pid, int motivo);
static void cambio_a_exit(t_pcb* pcb, t_colas* colas, int motivo);
static t_pcb* cambio_sacar_new(char* archivo_instrucciones, int prioridad,
                               t_colas* colas);
static void actualizar_mayor_prioridad_ready_sin_mutex(t_cola_ready* ready);
static t_pcb* cambio_sacar_ready_siguiente(t_cola_ready* ready);
static void cambio_sacar_ready(t_pcb* pcb, t_cola_ready* ready);
static void cambio_sacar_exec(t_pcb* pcb, t_lista_execute* exec,
                              t_contador* contador_syscalls);
static t_pcb* cambio_sacar_exec_siguiente(t_lista_execute* exec);
static void cambio_sacar_block(t_pcb* pcb, t_lista* block);
static t_pcb* cambio_sacar_block_siguiente(t_lista* block);
static void cambio_sacar_susp_block(t_pcb* pcb, t_lista* susp_block);
static t_pcb* cambio_sacar_susp_block_siguiente(t_lista* susp_block);
static void cambio_sacar_susp_ready(t_pcb* pcb, t_lista* susp_ready);
static t_pcb* cambio_sacar_susp_ready_siguiente(t_lista* susp_ready);
static void cambio_block_ready_sin_mutex(t_pcb* pcb, t_colas* colas);
static bool recibir_respuesta_suspender_proceso(t_colas* colas);
static bool avisar_proceso_suspendido(t_pcb* pcb, t_colas* colas);
static void cambio_block_susp_block_sin_mutex(t_pcb* pcb, t_colas* colas);
static void cambio_susp_block_susp_ready_sin_mutex(t_pcb* pcb, t_colas* colas);
static bool avisar_proceso_des_suspendido(t_pcb* pcb, t_colas* colas);
static bool puede_des_suspender(t_pcb* pcb, t_colas* colas);
static bool cambio_susp_ready_ready_sin_mutex(t_pcb* pcb, t_colas* colas);
static bool cambio_cualquiera_exit(t_colas* colas, int estado, int motivo);
static void bloquear_hilo_suspendido(t_datos_hilo_suspendido* datos,
                                     t_lista* lista);
static void desbloquear_hilo_suspendido(t_datos_hilo_suspendido* datos);
static void esperar_desbloqueo(t_datos_hilo_suspendido* datos);
static t_pcb* obtener_proceso_bloqueado(t_colas* colas,
                                        t_datos_hilo_suspensor* datos);
static void suspender_proceso(t_colas* colas, t_datos_hilo_suspensor* datos,
                              t_pcb* proceso);
static void esperar_proceso_bloqueado(t_colas* colas,
                                      t_datos_hilo_suspensor* datos);
static t_pcb* obtener_proceso_susp_ready(t_colas* colas,
                                         t_datos_hilo_des_suspensor* datos);
static void des_suspender_proceso(t_colas* colas,
                                  t_datos_hilo_des_suspensor* datos,
                                  t_pcb* proceso);
static void esperar_proceso_susp_ready(t_colas* colas,
                                       t_datos_hilo_des_suspensor* datos);
static void* hilo_suspensor(void* datos_void);
static void* hilo_des_suspensor(void* datos_void);
// funciones de Bloqueo/Desbloqueo total
static void bloqueo_total(t_colas* colas);
static bool entra_proceso(t_colas* colas, t_pcb* proceso);
static bool esta_vacia(t_lista* lista);
static bool retirar_de_la_lista(t_colas* colas);
static void rutina_des_suspension(t_colas* colas);
static int recibir_espacio(t_colas* colas);
static int recibir_tamanio(t_colas* colas);
static int recibir_tamanio_sin_logger(t_colas* colas);
static int tamanio_proceso_sin_mutex_ni_logger(t_colas* colas, uint32_t pid);
static int tamanio_proceso_sin_logger(t_colas* colas, uint32_t pid);
static void* hilo_rutina_des_suspension(void* datos_des_suspension);
static bool esta_des_suspendiendo_set(t_colas* colas, bool nuevo_estado);
static bool esta_compactando_set(t_colas* colas, bool nuevo_estado);
// RUTINA DE COMPACTACIÓN
static void* hilo_desbloquear_cola_ready(void* args);
static void crear_hilo_desbloquear_cola_ready(t_colas* colas);
static bool termino_compactacion(t_colas* colas);
static void compactacion(t_colas* colas);
static bool avisar_nuevo_proceso(t_colas* colas, char* archivo_instrucciones,
                                 uint32_t pid);

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
      inicializar_contador_procesos(socket_servidor, logger, socket_km);
  colas->contador_hilos = crear_contador();
  colas->contador_syscalls = crear_contador();
  pthread_mutex_init(&(colas->mutex_rutina), NULL);
  colas->terminar_rutinas = false;
  colas->logger = logger;
  colas->socket_km = socket_km;
  colas->socket_servidor = socket_servidor;
  pthread_mutex_init(&(colas->mutex_compactacion_activa), NULL);
  colas->compactacion_activa = false;
  pthread_mutex_init(&(colas->mutex_des_suspension_activa), NULL);
  colas->des_suspension_activa = false;
  iniciar_hilos_suspendido(colas, suspension_timeout);
  return colas;
}

void destruir_colas(t_colas* colas)
{
  terminar_rutinas(colas);
  esperar_contador_hilos(colas);
  destruir_contador_hilos(colas);
  destruir_contador_syscalls(colas);
  terminar_hilos_suspendido(colas);
  destruir_hilos_suspendido(colas);
  destruir_cola_ready(&(colas->ready));
  destruir_lista_exec(&(colas->exec));
  destruir_lista(&(colas->block));
  destruir_lista(&(colas->susp_block));
  destruir_lista(&(colas->susp_ready));
  destruir_contador_procesos(colas->contador_procesos);
  pthread_mutex_destroy(&(colas->mutex_rutina));
  pthread_mutex_destroy(&(colas->mutex_compactacion_activa));
  pthread_mutex_destroy(&(colas->mutex_des_suspension_activa));
  free(colas);
}

bool esta_cola_ready_vacia(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  bool cant = ready->cant_procesos_ready;
  pthread_mutex_unlock(&(ready->mutex_cola));
  return cant == 0;
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
  ready->desalojar_todo = false;
  pthread_mutex_unlock(&(ready->bloquear_salida));
  pthread_mutex_lock(&(ready->mutex_cola));
  pthread_cond_broadcast(&(ready->salida_desbloqueada));
  pthread_mutex_unlock(&(ready->mutex_cola));
}

bool cola_ready_terminada(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_terminar_cola));
  bool ret = ready->terminar_cola;
  pthread_mutex_unlock(&(ready->mutex_terminar_cola));
  return ret;
}

void terminar_cola_ready(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_terminar_cola));
  ready->terminar_cola = true;
  pthread_mutex_unlock(&(ready->mutex_terminar_cola));
  pthread_mutex_lock(&(ready->mutex_cola));
  pthread_cond_broadcast(&(ready->nuevo_proceso));
  pthread_cond_broadcast(&(ready->salida_desbloqueada));
  pthread_mutex_unlock(&(ready->mutex_cola));
}

void esperar_cola_exec_vacia(t_colas* colas)
{
  pthread_mutex_lock(&(colas->exec.mutex_lista));
  while (list_size(colas->exec.lista) > 0)
  {
    pthread_cond_wait(&(colas->exec.cola_vacia), &(colas->exec.mutex_lista));
  }
  pthread_mutex_unlock(&(colas->exec.mutex_lista));
}

void esperar_cola_exec_vacia_con_syscalls(t_colas* colas)
{
  pthread_mutex_lock(&(colas->exec.mutex_lista));
  pthread_mutex_lock(&(colas->contador_syscalls->mutex_contador));
  while (list_size(colas->exec.lista) - colas->contador_syscalls->cantidad > 0)
  {
    pthread_cond_wait(&(colas->contador_syscalls->condicion),
                      &(colas->exec.mutex_lista));
  }
  pthread_mutex_unlock(&(colas->contador_syscalls->mutex_contador));
  pthread_mutex_unlock(&(colas->exec.mutex_lista));
}

bool puedo_suspender(t_pcb* pcb, int suspension_timeout)
{
  return time_diff(millis(), pcb->tiempo_bloqueado) >= suspension_timeout;
}

void actualizar_prioridad(t_pcb* pcb, t_colas* colas)
{
  if (!colas->ready.cola_multi_nivel)
  {
    return;
  }

  pthread_mutex_lock(&(pcb->mutex_estado));
  if (pcb->estado == EST_READY)
  {
    cambio_sacar_ready(pcb, &(colas->ready));
    cambio_a_ready(pcb, &(colas->ready));
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_a_exec(t_pcb* pcb, t_lista_execute* exec)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  if (exec->desalojo)
  {
    t_pcb* prioridad_mas_baja = exec->prioridad_mas_baja;

    int valor_prio_baja = -1;

    if (prioridad_mas_baja != NULL)
    {
      pthread_mutex_lock(&(prioridad_mas_baja->mutex_prioridad));
      valor_prio_baja = prioridad_mas_baja->prioridad;
      pthread_mutex_unlock(&(prioridad_mas_baja->mutex_prioridad));
    }
    pthread_mutex_lock(&(pcb->mutex_prioridad));
    int valor_prio_pcb = pcb->prioridad;
    pthread_mutex_unlock(&(pcb->mutex_prioridad));
    if (prioridad_mas_baja == NULL || valor_prio_baja >= valor_prio_pcb)
    {
      exec->prioridad_mas_baja = pcb;
    }
  }
  list_add(exec->lista, pcb);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

t_pcb* cambio_sacar_ready_bloqueante(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  pthread_mutex_lock(&(ready->bloquear_salida));
  while (!cola_ready_terminada(ready) &&
         (ready->cant_procesos_ready == 0 || ready->desalojar_todo))
  {
    if (!cola_ready_terminada(ready) && ready->cant_procesos_ready == 0)
    {
      pthread_mutex_unlock(&(ready->bloquear_salida));
      pthread_cond_wait(&(ready->nuevo_proceso), &(ready->mutex_cola));
      pthread_mutex_lock(&(ready->bloquear_salida));
    }
    if (!cola_ready_terminada(ready) && ready->desalojar_todo)
    {
      pthread_mutex_unlock(&(ready->bloquear_salida));
      pthread_cond_wait(&(ready->salida_desbloqueada), &(ready->mutex_cola));
      pthread_mutex_lock(&(ready->bloquear_salida));
    }
  }
  pthread_mutex_unlock(&(ready->bloquear_salida));

  t_pcb* pcb;
  if (cola_ready_terminada(ready))
  {
    pcb = NULL;
  }
  else
  {
    pcb = cambio_sacar_ready_siguiente_sin_mutex(ready);
  }
  pthread_mutex_unlock(&(ready->mutex_cola));
  return pcb;
}

t_pcb* cambio_sacar_ready_siguiente_sin_mutex(t_cola_ready* ready)
{
  for (int i = 0; i < ready->cantidad_colas; i++)
  {
    if (!list_is_empty(ready->colas[i].cola))
    {
      ready->cant_procesos_ready--;
      t_pcb* pcb = list_remove(ready->colas[i].cola, 0);
      if (list_is_empty(ready->colas[i].cola))
      {
        actualizar_mayor_prioridad_ready_sin_mutex(ready);
      }
      if (ready->cant_procesos_ready == 0)
      {
        pthread_cond_signal(&(ready->cola_vacia));
      }
      return pcb;
    }
  }
  return NULL;
}

void cambio_ready_exec(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  gestionar_estado_pcb(colas->logger, pcb, EST_READY, EST_EXEC);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_new_ready(t_colas* colas, char* archivo_instrucciones,
                      int prioridad)
{
  logger_info(colas->logger, "Creando proceso de prioridad %d ubicado en %s",
              prioridad, archivo_instrucciones);
  t_pcb* pcb = cambio_sacar_new(archivo_instrucciones, prioridad, colas);
  if (pcb != NULL)
  {
    if (!check_prioridad_valida(pcb, &(colas->ready), colas->logger))
    {
      log_cambio_estado(colas->logger, pcb->pid, EST_NEW, EST_EXIT);
      cambio_a_exit(pcb, colas, MFP_PRIORIDAD_NO_VALIDA);
    }
    else
    {
      pcb->estado = EST_READY;
      log_cambio_estado(colas->logger, pcb->pid, EST_NEW, EST_READY);
      cambio_a_ready(pcb, &(colas->ready));
    }
  }
}

void cambio_exec_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_READY))
  {
    cambio_sacar_exec(pcb, &(colas->exec), colas->contador_syscalls);
    cambio_a_ready(pcb, &(colas->ready));
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_exec_exit(t_pcb* pcb, t_colas* colas, int motivo)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_EXIT))
  {
    cambio_sacar_exec(pcb, &(colas->exec), colas->contador_syscalls);
    pthread_mutex_unlock(&(pcb->mutex_estado));
    cambio_a_exit(pcb, colas, motivo);
  }
  else
  {
    pthread_mutex_unlock(&(pcb->mutex_estado));
  }
}

void cambio_exec_block(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (gestionar_estado_pcb(colas->logger, pcb, EST_EXEC, EST_BLOCK))
  {
    cambio_sacar_exec(pcb, &(colas->exec), colas->contador_syscalls);
    cambio_a_block(pcb, &(colas->block));
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

void cambio_block_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_block_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
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

void cambio_susp_block_susp_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  cambio_susp_block_susp_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
}

bool cambio_susp_ready_ready(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  bool ret = cambio_susp_ready_ready_sin_mutex(pcb, colas);
  pthread_mutex_unlock(&(pcb->mutex_estado));
  return ret;
}

void cambio_desbloquear(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(pcb->mutex_estado));
  if (pcb->estado == EST_BLOCK)
  {
    cambio_block_ready_sin_mutex(pcb, colas);
  }
  else
  {
    cambio_susp_block_susp_ready_sin_mutex(pcb, colas);
  }
  pthread_mutex_unlock(&(pcb->mutex_estado));
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

void bloquear_hilos_suspendido(t_colas* colas)
{
  bloquear_hilo_suspendido(colas->datos_suspendido->datos_hilo_suspensor->datos,
                           &(colas->block));
  bloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos,
      &(colas->susp_ready));
  logger_info(colas->logger, "Hilos suspendido bloqueados");
}

void desbloquear_hilos_suspendido(t_colas* colas)
{
  desbloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_suspensor->datos);
  desbloquear_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
  logger_info(colas->logger, "Hilos suspendido desbloqueados");
}

int espacio_disponible_sin_mutex(t_colas* colas, uint32_t pid)
{
  if (!(enviar_string(OP_PEDIR_MEMORIA_DISPONIBLE,
                      "Solicito el espacio disponible",
                      colas->socket_km->socket_km)))
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return -1;
  }
  return recibir_espacio(colas);
}

int espacio_disponible(t_colas* colas, uint32_t pid)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  int espacio = espacio_disponible_sin_mutex(colas, pid);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return espacio;
}

int tamanio_proceso_sin_mutex(t_colas* colas, uint32_t pid)
{
  if (!(enviar_buffer(OP_PEDIR_TAMANIO_PROCESO, &pid, sizeof(uint32_t),
                      colas->socket_km->socket_km)))
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return -1;
  }

  return recibir_tamanio(colas);
}
static int tamanio_proceso_sin_mutex_ni_logger(t_colas* colas, uint32_t pid)
{
  if (!(enviar_buffer(OP_PEDIR_TAMANIO_PROCESO, &pid, sizeof(uint32_t),
                      colas->socket_km->socket_km)))
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return -1;
  }

  return recibir_tamanio_sin_logger(colas);
}

int tamanio_proceso(t_colas* colas, uint32_t pid)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  int espacio = tamanio_proceso_sin_mutex(colas, pid);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return espacio;
}

static int tamanio_proceso_sin_logger(t_colas* colas, uint32_t pid)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  int espacio = tamanio_proceso_sin_mutex_ni_logger(colas, pid);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return espacio;
}
// usar para memoria liberada, nuevo stick o fin de compactación
void crear_hilo_rutina_des_suspension(t_colas* colas)
{
  if (esta_compactando(colas) || esta_des_suspendiendo_set(colas, true))
  {
    return;
  }

  pthread_t hilo;
  if (pthread_create(&hilo, NULL, hilo_rutina_des_suspension, colas) != 0)
  {
    logger_error(
        colas->logger,
        "Error en la creación del hilo de la rutina de des-suspension");
  }
  else
  {
    pthread_detach(hilo);
    logger_info(colas->logger,
                "Hilo de la rutina de des-suspension iniciado exitosamente");
  }
}

void rutina_compactacion(t_colas* colas)
{
  if (esta_compactando_set(colas, true))
  {
    return;
  }

  pthread_mutex_lock(&(colas->mutex_rutina));
  if (!colas->terminar_rutinas)
  {
    bloqueo_total(colas);
    compactacion(colas);
    esta_compactando_set(colas, false);
    crear_hilo_desbloquear_cola_ready(colas);
    crear_hilo_rutina_des_suspension(colas);
  }
  pthread_mutex_unlock(&(colas->mutex_rutina));
}

bool esta_compactando(t_colas* colas)
{
  pthread_mutex_lock(&(colas->mutex_compactacion_activa));
  bool ret = colas->compactacion_activa;
  pthread_mutex_unlock(&(colas->mutex_compactacion_activa));

  return ret;
}

bool esta_des_suspendiendo(t_colas* colas)
{
  pthread_mutex_lock(&(colas->mutex_des_suspension_activa));
  bool ret = colas->des_suspension_activa;
  pthread_mutex_unlock(&(colas->mutex_des_suspension_activa));

  return ret;
}

void sumar_contador_syscalls(t_colas* colas)
{
  sumar_contador(colas->contador_syscalls);
}

void restar_contador_syscalls(t_colas* colas)
{
  pthread_mutex_lock(&(colas->contador_syscalls->mutex_contador));
  colas->contador_syscalls->cantidad--;
  pthread_mutex_unlock(&(colas->contador_syscalls->mutex_contador));
}

static t_contador* crear_contador(void)
{
  t_contador* contador = malloc(sizeof(t_contador));
  contador->cantidad = 0;
  pthread_mutex_init(&(contador->mutex_contador), NULL);
  pthread_cond_init(&(contador->condicion), NULL);
  return contador;
}

static void sumar_contador(t_contador* contador)
{
  pthread_mutex_lock(&(contador->mutex_contador));
  contador->cantidad++;
  pthread_mutex_unlock(&(contador->mutex_contador));
}

static void sumar_contador_hilos(t_colas* colas)
{
  sumar_contador(colas->contador_hilos);
}

static void restar_contador_hilos(t_colas* colas)
{
  pthread_mutex_lock(&(colas->contador_hilos->mutex_contador));
  colas->contador_hilos->cantidad--;
  if (colas->contador_hilos->cantidad <= 0)
  {
    pthread_cond_signal(&(colas->contador_hilos->condicion));
  }
  pthread_mutex_unlock(&(colas->contador_hilos->mutex_contador));
}

static void esperar_contador_hilos(t_colas* colas)
{
  logger_info(colas->logger, "Esperando a que finalicen todos los hilos");
  pthread_mutex_lock(&(colas->contador_hilos->mutex_contador));
  while (colas->contador_hilos->cantidad > 0)
  {
    pthread_cond_wait(&(colas->contador_hilos->condicion),
                      &(colas->contador_hilos->mutex_contador));
  }
  pthread_mutex_unlock(&(colas->contador_hilos->mutex_contador));
  logger_info(colas->logger, "Hilos finalizados");
}

static void destruir_contador(t_contador* contador)
{
  pthread_mutex_destroy(&(contador->mutex_contador));
  pthread_cond_destroy(&(contador->condicion));
  free(contador);
}

static void destruir_contador_hilos(t_colas* colas)
{
  destruir_contador(colas->contador_hilos);
}

static void destruir_contador_syscalls(t_colas* colas)
{
  destruir_contador(colas->contador_syscalls);
}

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
      cola->colas[i].cola = list_create();
    }
    list_iterator_destroy(iterator_algoritmos);
  }
  else
  {
    cola->cola_multi_nivel = false;
    cola->cantidad_colas = 1;
    cola->colas = malloc(sizeof(t_cola_individual_ready));
    cola->colas->cola = list_create();
    cola->colas->algoritmo = algoritmo;
  }
  pthread_mutex_init(&(cola->mutex_cola), NULL);
  pthread_cond_init(&(cola->nuevo_proceso), NULL);
  pthread_mutex_init(&(cola->bloquear_salida), NULL);
  pthread_mutex_init(&(cola->mutex_terminar_cola), NULL);
  pthread_cond_init(&(cola->salida_desbloqueada), NULL);
  pthread_cond_init(&(cola->cola_vacia), NULL);
  cola->desalojar_todo = false;
  cola->cant_procesos_ready = 0;
  cola->mayor_prioridad = INT_MAX;
  cola->terminar_cola = false;
}

static void inicializar_lista_exec(t_lista_execute* lista, int quantum,
                                   bool desalojo)
{
  lista->lista = list_create();
  pthread_mutex_init(&(lista->mutex_lista), NULL);
  pthread_cond_init(&(lista->cola_vacia), NULL);
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
          &(colas->datos_suspendido->datos_hilo_suspensor->datos->hilo), NULL,
          hilo_suspensor, colas) != 0)
  {
    logger_error(colas->logger, "Error en la creación del hilo suspensor");
  }
  else
  {
    logger_info(colas->logger, "Hilo suspensor iniciado exitosamente");
  }
}

static void iniciar_hilo_des_suspensor(t_colas* colas)
{
  if (pthread_create(
          &(colas->datos_suspendido->datos_hilo_des_suspensor->datos->hilo),
          NULL, hilo_des_suspensor, colas) != 0)
  {
    logger_error(colas->logger, "Error en la creación del hilo des-suspensor");
  }
  else
  {
    logger_info(colas->logger, "Hilo des-suspensor iniciado exitosamente");
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

static void destruir_cola_ready(t_cola_ready* cola)
{
  for (int i = 0; i < cola->cantidad_colas; i++)
  {
    list_destroy(cola->colas[i].cola);
  }
  pthread_cond_destroy(&(cola->nuevo_proceso));
  pthread_cond_destroy(&(cola->salida_desbloqueada));
  pthread_mutex_destroy(&(cola->mutex_cola));
  pthread_mutex_destroy(&(cola->bloquear_salida));
  pthread_cond_destroy(&(cola->cola_vacia));
  pthread_mutex_destroy(&(cola->mutex_terminar_cola));
  free(cola->colas);
}

static void destruir_lista_exec(t_lista_execute* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
  pthread_cond_destroy(&(lista->cola_vacia));
}

static void destruir_lista(t_lista* lista)
{
  list_destroy(lista->lista);
  pthread_mutex_destroy(&(lista->mutex_lista));
  pthread_cond_destroy(&(lista->cond_nuevo_proceso));
}

static void terminar_hilo_suspendido(t_datos_hilo_suspendido* datos,
                                     t_lista* lista)
{
  pthread_mutex_lock(&(datos->mutex_estado));
  pthread_mutex_lock(&(lista->mutex_lista));
  pthread_cond_signal(datos->esperar_proceso);
  pthread_cond_signal(&(datos->desbloquear));
  pthread_mutex_unlock(&(lista->mutex_lista));
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

static void terminar_hilos_suspendido(t_colas* colas)
{
  terminar_hilo_suspendido(colas->datos_suspendido->datos_hilo_suspensor->datos,
                           &(colas->block));
  terminar_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos,
      &(colas->susp_ready));
  esperar_hilo_suspendido(colas->datos_suspendido->datos_hilo_suspensor->datos);
  esperar_hilo_suspendido(
      colas->datos_suspendido->datos_hilo_des_suspensor->datos);
}

static void destruir_hilos_suspendido(t_colas* colas)
{
  destruir_hilo_suspensor(colas->datos_suspendido->datos_hilo_suspensor);
  destruir_hilo_des_suspensor(
      colas->datos_suspendido->datos_hilo_des_suspensor);
  free(colas->datos_suspendido);
}

static void terminar_rutinas(t_colas* colas)
{
  pthread_mutex_lock(&(colas->mutex_rutina));
  colas->terminar_rutinas = true;
  pthread_mutex_unlock(&(colas->mutex_rutina));
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
               "## %u No puede pasar del estado %s al estado %s porque se "
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
      t_pcb* actual_mas_baja = exec->prioridad_mas_baja;
      pthread_mutex_lock(&(actual_mas_baja->mutex_prioridad));
      int prioridad_actual = actual_mas_baja->prioridad;
      pthread_mutex_unlock(&(actual_mas_baja->mutex_prioridad));

      pthread_mutex_lock(&(pcb->mutex_prioridad));
      int prioriad_iterador = pcb->prioridad;
      pthread_mutex_unlock(&(pcb->mutex_prioridad));

      if (prioridad_actual > prioriad_iterador)
      {
        exec->prioridad_mas_baja = pcb;
      }
    }
  }
  list_iterator_destroy(iterator_lista);
  pthread_mutex_unlock(&(exec->mutex_lista));
}

static void cambio_a_ready(t_pcb* pcb, t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  int prioridad = get_prioridad_pcb(pcb);
  if (ready->cant_procesos_ready == 0 || ready->mayor_prioridad > prioridad)
  {
    ready->mayor_prioridad = prioridad;
  }

  if (ready->cant_procesos_ready == 0)
  {
    pthread_cond_signal(&(ready->nuevo_proceso));
  }
  ready->cant_procesos_ready++;

  if (ready->cola_multi_nivel)
  {
    list_add(ready->colas[prioridad].cola, pcb);
  }
  else
  {
    list_add(ready->colas->cola, pcb);
  }
  pthread_mutex_unlock(&(ready->mutex_cola));
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
  insertar_pcb_en_orden(susp_block->lista, pcb);
  pthread_mutex_unlock(&(susp_block->mutex_lista));
}

static void cambio_a_susp_ready(t_pcb* pcb, t_lista* susp_ready)
{
  pthread_mutex_lock(&(susp_ready->mutex_lista));
  if (list_size(susp_ready->lista) == 0)
  {
    pthread_cond_signal(&(susp_ready->cond_nuevo_proceso));
  }
  insertar_pcb_en_orden(susp_ready->lista, pcb);
  pthread_mutex_unlock(&(susp_ready->mutex_lista));
}

static void log_cambio_a_exit(t_logger* logger, uint32_t pid, int motivo)
{
  logger_info(logger, "## %u finalizó su ejecución con motivo de %s", pid,
              MOTIVOS_FIN_PROCESO[motivo]);
}

static void cambio_a_exit(t_pcb* pcb, t_colas* colas, int motivo)
{
  if (motivo != MFP_CIERRE_SISTEMA && motivo != MFP_PRIORIDAD_NO_VALIDA)
  {
    bool ejecutar_rutina_des_suspender = tamanio_proceso(colas, pcb->pid) > 0;
    if (avisar_terminar_proceso(colas->socket_km, pcb->pid,
                                colas->socket_servidor, colas->logger) &&
        ejecutar_rutina_des_suspender)
    {
      crear_hilo_rutina_des_suspension(colas);
    }
  }
  log_cambio_a_exit(colas->logger, pcb->pid, motivo);
  destruir_pcb(pcb);
  disminuir_contador_procesos(colas->contador_procesos);
}

static t_pcb* cambio_sacar_new(char* archivo_instrucciones, int prioridad,
                               t_colas* colas)
{
  t_pcb* pcb = crear_pcb(EST_NEW, prioridad);
  logger_info(colas->logger, "## %u Se crea el proceso - Estado: NEW",
              pcb->pid);

  aumentar_contador_procesos(colas->contador_procesos);
  if (!avisar_nuevo_proceso(colas, archivo_instrucciones, pcb->pid))
  {
    log_cambio_estado(colas->logger, pcb->pid, EST_NEW, EST_EXIT);
    cambio_a_exit(pcb, colas, MFP_CIERRE_SISTEMA);

    return NULL;
  }
  return pcb;
}

static void actualizar_mayor_prioridad_ready_sin_mutex(t_cola_ready* ready)
{
  if (ready->cant_procesos_ready > 0 && ready->cola_multi_nivel)
  {
    for (int i = 0; i < ready->cantidad_colas; i++)
    {
      if (!list_is_empty(ready->colas[i].cola))
      {
        ready->mayor_prioridad = i;
        return;
      }
    }
  }
  ready->mayor_prioridad = ready->cantidad_colas;
}

static void cambio_sacar_ready(t_pcb* pcb, t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  int pos = ready->cola_multi_nivel ? get_prioridad_pcb(pcb) : 0;
  list_remove_element(ready->colas[pos].cola, pcb);
  actualizar_mayor_prioridad_ready_sin_mutex(ready);
  ready->cant_procesos_ready--;
  if (ready->cant_procesos_ready == 0)
  {
    pthread_cond_signal(&(ready->cola_vacia));
  }
  pthread_mutex_unlock(&(ready->mutex_cola));
}

static t_pcb* cambio_sacar_ready_siguiente(t_cola_ready* ready)
{
  pthread_mutex_lock(&(ready->mutex_cola));
  t_pcb* pcb = cambio_sacar_ready_siguiente_sin_mutex(ready);
  pthread_mutex_unlock(&(ready->mutex_cola));
  return pcb;
}

static void cambio_sacar_exec(t_pcb* pcb, t_lista_execute* exec,
                              t_contador* contador_syscalls)
{
  pthread_mutex_lock(&(exec->mutex_lista));
  list_remove_element(exec->lista, pcb);
  bool actualizar_elemento = pcb == exec->prioridad_mas_baja;
  if (list_size(exec->lista) == 0)
  {
    pthread_cond_signal(&(exec->cola_vacia));
  }
  pthread_mutex_lock(&(contador_syscalls->mutex_contador));
  if (list_size(exec->lista) - contador_syscalls->cantidad == 0)
  {
    pthread_cond_signal(&(contador_syscalls->condicion));
  }
  pthread_mutex_unlock(&(contador_syscalls->mutex_contador));
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

static void cambio_block_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_BLOCK, EST_READY))
  {
    cambio_sacar_block(pcb, &(colas->block));
    cambio_a_ready(pcb, &(colas->ready));
  }
}

static bool recibir_respuesta_suspender_proceso(t_colas* colas)
{
  int op_code = recibir_operacion(colas->socket_km->socket_km);
  switch (op_code)
  {
    case OP_SUSPENSION_EXITOSA:
      free(recibir_string(colas->socket_km->socket_km));
      return true;
    case OP_SUSPENSION_NO_EXITOSA:
      break;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return recibir_respuesta_suspender_proceso(colas);
    case OP_MEMORIA_CORRUPTA:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      break;
    default:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      break;
  }
  free(recibir_string(colas->socket_km->socket_km));
  return false;
}

static bool avisar_proceso_suspendido(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  if (!enviar_buffer(OP_SUSPENDER_PROCESO, &(pcb->pid), sizeof(uint32_t),
                     colas->socket_km->socket_km))
  {
    pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return false;
  }

  bool ret = recibir_respuesta_suspender_proceso(colas);
  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return ret;
}

static void cambio_block_susp_block_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (pcb->estado != EST_BLOCK)
  {
    log_estado_no_valido(colas->logger, pcb->pid, pcb->estado, EST_BLOCK,
                         EST_SUSP_BLOCK);
    return;
  }

  if (!avisar_proceso_suspendido(pcb, colas))
  {
    logger_info(colas->logger, "No se pudo suspender el proceso %u", pcb->pid);
    return;
  }

  log_cambio_estado(colas->logger, pcb->pid, EST_BLOCK, EST_SUSP_BLOCK);
  pcb->estado = EST_SUSP_BLOCK;

  cambio_sacar_block(pcb, &(colas->block));
  cambio_a_susp_block(pcb, &(colas->susp_block));

  pthread_mutex_lock(&(colas->mutex_compactacion_activa));
  pthread_mutex_lock(&(colas->mutex_des_suspension_activa));
  if (!colas->compactacion_activa && !colas->des_suspension_activa)
  {
    desbloquear_hilo_suspendido(
        colas->datos_suspendido->datos_hilo_des_suspensor->datos);
  }
  pthread_mutex_unlock(&(colas->mutex_des_suspension_activa));
  pthread_mutex_unlock(&(colas->mutex_compactacion_activa));
}

static void cambio_susp_block_susp_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (gestionar_estado_pcb(colas->logger, pcb, EST_SUSP_BLOCK, EST_SUSP_READY))
  {
    cambio_sacar_susp_block(pcb, &(colas->susp_block));
    cambio_a_susp_ready(pcb, &(colas->susp_ready));
  }
}

static bool avisar_proceso_des_suspendido(t_pcb* pcb, t_colas* colas)
{
  pthread_mutex_lock(&(colas->socket_km->mutex_socket));

  if (!(enviar_buffer(OP_DES_SUSPENDER_PROCESO, &(pcb->pid), sizeof(uint32_t),
                      colas->socket_km->socket_km)))
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return false;
  }

  bool ret = entra_proceso(colas, pcb);

  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));

  return ret;
}

static bool puede_des_suspender(t_pcb* pcb, t_colas* colas)
{
  return espacio_disponible(colas, pcb->pid) >=
         tamanio_proceso(colas, pcb->pid);
}

static bool cambio_susp_ready_ready_sin_mutex(t_pcb* pcb, t_colas* colas)
{
  if (pcb->estado != EST_SUSP_READY)
  {
    log_estado_no_valido(colas->logger, pcb->pid, pcb->estado, EST_SUSP_READY,
                         EST_READY);
    return false;
  }

  if (!puede_des_suspender(pcb, colas))
  {
    logger_info(colas->logger,
                "No hay suficiente espacio para des-suspender al proceso %u",
                pcb->pid);
    return false;
  }

  if (!avisar_proceso_des_suspendido(pcb, colas))
  {
    logger_info(colas->logger, "No es posible des-suspender el proceso %u",
                pcb->pid);
    return false;
  }

  log_cambio_estado(colas->logger, pcb->pid, EST_SUSP_READY, EST_READY);
  pcb->estado = EST_READY;
  cambio_sacar_susp_ready(pcb, &(colas->susp_ready));
  cambio_a_ready(pcb, &(colas->ready));
  return true;
}

static bool cambio_cualquiera_exit(t_colas* colas, int estado, int motivo)
{
  t_pcb* pcb = NULL;
  switch (estado)
  {
    case EST_READY:
      pcb = cambio_sacar_ready_siguiente(&(colas->ready));
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
  cambio_a_exit(pcb, colas, motivo);
  return true;
}

static void bloquear_hilo_suspendido(t_datos_hilo_suspendido* datos,
                                     t_lista* lista)
{
  pthread_mutex_lock(&(datos->mutex_estado));
  switch (datos->estado)
  {
    case EH_EJECUTANDO:
      datos->estado = EH_BLOQUEADO;
      break;
    case EH_ESPERANDO_PROCESO:
      pthread_mutex_lock(&(lista->mutex_lista));
      datos->estado = EH_BLOQUEADO;
      pthread_cond_signal(datos->esperar_proceso);
      pthread_mutex_unlock(&(lista->mutex_lista));
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

static void esperar_desbloqueo(t_datos_hilo_suspendido* datos)
{
  while (datos->estado == EH_BLOQUEADO)
  {
    pthread_cond_wait(&(datos->desbloquear), &(datos->mutex_estado));
  }
}

static t_pcb* obtener_proceso_bloqueado(t_colas* colas,
                                        t_datos_hilo_suspensor* datos)
{
  pthread_mutex_lock(&(colas->block.mutex_lista));
  t_pcb* proceso = NULL;
  int tamanio_en_memoria = -1;
  t_list_iterator* iterador = list_iterator_create(colas->block.lista);
  while (list_iterator_has_next(iterador))
  {
    proceso = list_iterator_next(iterador);
    tamanio_en_memoria = tamanio_proceso_sin_logger(colas, proceso->pid);
    if (tamanio_en_memoria > 0)
    {
      break;
    }
  }
  pthread_mutex_unlock(&(colas->block.mutex_lista));
  if (proceso == NULL)
  {
    datos->datos->estado = EH_ESPERANDO_PROCESO;
  }
  list_iterator_destroy(iterador);
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
    usleep(tiempo_sleep > 500 ? 500 : tiempo_sleep * 1000);
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
                      &(colas->block.mutex_lista));
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

  bool exitoso = false;
  if (proceso->estado == EST_SUSP_READY)
  {
    exitoso = cambio_susp_ready_ready_sin_mutex(proceso, colas);
  }

  pthread_mutex_unlock(&(proceso->mutex_estado));

  if (!exitoso)
  {
    bloquear_hilo_suspendido(datos->datos, &(colas->susp_ready));
  }

  pthread_mutex_lock(&(datos->datos->mutex_estado));
}

static void esperar_proceso_susp_ready(t_colas* colas,
                                       t_datos_hilo_des_suspensor* datos)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(colas->susp_ready.mutex_lista));
  if (list_is_empty(colas->susp_ready.lista))
  {
    pthread_cond_wait(datos->datos->esperar_proceso,
                      &(colas->susp_ready.mutex_lista));
  }
  bool lista_vacia = list_is_empty(colas->susp_ready.lista);
  pthread_mutex_unlock(&(colas->susp_ready.mutex_lista));
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

// funciones de Bloqueo/Desbloqueo total
static void bloqueo_total(t_colas* colas)
{
  bloquear_cola_ready(&(colas->ready));
  bloquear_hilos_suspendido(colas);
  esperar_cola_exec_vacia_con_syscalls(colas);
}

static bool entra_proceso(t_colas* colas, t_pcb* proceso)
{
  int op_code = recibir_operacion(colas->socket_km->socket_km);

  switch (op_code)
  {
    case OP_DES_SUSPENSION_NO_EXITOSA:
      free(recibir_string(colas->socket_km->socket_km));
      return false;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return entra_proceso(colas, proceso);
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      return false;
    case OP_DES_SUSPENSION_EXITOSA:
      free(recibir_string(colas->socket_km->socket_km));
      return true;
    default:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      return false;
  }
}

static bool esta_vacia(t_lista* lista)
{
  pthread_mutex_lock(&(lista->mutex_lista));
  return list_is_empty(lista->lista);
}

static bool retirar_de_la_lista(t_colas* colas)
{
  t_pcb* proceso = list_get(colas->susp_ready.lista, 0);
  pthread_mutex_unlock(&(colas->susp_ready.mutex_lista));
  pthread_mutex_lock(&(proceso->mutex_estado));
  if (proceso->estado == EST_SUSP_READY)
  {
    bool resultado = cambio_susp_ready_ready_sin_mutex(proceso, colas);
    pthread_mutex_unlock(&(proceso->mutex_estado));
    return resultado;
  }

  pthread_mutex_unlock(&(proceso->mutex_estado));
  return true;
}

static void rutina_des_suspension(t_colas* colas)
{
  bool seguir_operando = true;

  while (!esta_vacia(&(colas->susp_ready)) && seguir_operando)
  {
    if (esta_compactando(colas))
    {
      pthread_mutex_unlock(&(colas->susp_ready.mutex_lista));
      break;
    }

    seguir_operando = retirar_de_la_lista(colas);
  }
  pthread_mutex_unlock(&(colas->susp_ready.mutex_lista));
}

static int recibir_espacio(t_colas* colas)
{
  int op_code = recibir_operacion(colas->socket_km->socket_km);

  switch (op_code)
  {
    case OP_MEMORIA_DISPONIBLE:
      int espacio;
      int* aux = recibir_buffer(&espacio, colas->socket_km->socket_km);
      espacio = *aux;
      free(aux);
      logger_info(colas->logger, "Espacio disponible: %d", espacio);
      return espacio;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return recibir_espacio(colas);
      break;
    case OP_MEMORIA_CORRUPTA:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      break;
    default:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      break;
  }
  return -1;
}

static int recibir_tamanio(t_colas* colas)
{
  int op_code = recibir_operacion(colas->socket_km->socket_km);

  switch (op_code)
  {
    case OP_TAMANIO_PROCESO:
      int espacio;
      int* aux = recibir_buffer(&espacio, colas->socket_km->socket_km);
      espacio = *aux;
      free(aux);
      logger_info(colas->logger, "Tamaño proceso: %d", espacio);
      return espacio;
      break;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return recibir_tamanio(colas);
      break;
    case OP_MEMORIA_CORRUPTA:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      break;
    default:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      break;
  }
  return -1;
}
static int recibir_tamanio_sin_logger(t_colas* colas)
{
  int op_code = recibir_operacion(colas->socket_km->socket_km);

  switch (op_code)
  {
    case OP_TAMANIO_PROCESO:
      int espacio;
      int* aux = recibir_buffer(&espacio, colas->socket_km->socket_km);
      espacio = *aux;
      free(aux);
      return espacio;
      break;
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return recibir_tamanio(colas);
      break;
    case OP_MEMORIA_CORRUPTA:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      break;
    default:
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      break;
  }
  return -1;
}

static void* hilo_rutina_des_suspension(void* datos_des_suspension)
{
  t_colas* colas = (t_colas*)datos_des_suspension;
  sumar_contador_hilos(colas);
  pthread_mutex_lock(&(colas->mutex_rutina));
  if (!colas->terminar_rutinas)
  {
    bloquear_hilos_suspendido(colas);
    rutina_des_suspension(colas);
    esta_des_suspendiendo_set(colas, false);
    desbloquear_hilos_suspendido(colas);
    logger_info(colas->logger, "Rutina de des-suspensión terminada");
  }
  pthread_mutex_unlock(&(colas->mutex_rutina));
  restar_contador_hilos(colas);
  return NULL;
}

static bool esta_des_suspendiendo_set(t_colas* colas, bool nuevo_estado)
{
  pthread_mutex_lock(&(colas->mutex_des_suspension_activa));
  bool ret = colas->des_suspension_activa;
  colas->des_suspension_activa = nuevo_estado;
  pthread_mutex_unlock(&(colas->mutex_des_suspension_activa));

  return ret;
}

static bool esta_compactando_set(t_colas* colas, bool nuevo_estado)
{
  pthread_mutex_lock(&(colas->mutex_compactacion_activa));
  bool ret = colas->compactacion_activa;
  colas->compactacion_activa = nuevo_estado;
  pthread_mutex_unlock(&(colas->mutex_compactacion_activa));

  return ret;
}

// RUTINA DE COMPACTACIÓN
static void* hilo_desbloquear_cola_ready(void* args)
{
  t_colas* colas = (t_colas*)args;
  sumar_contador_hilos(colas);

  esperar_cola_exec_vacia(colas);

  pthread_mutex_lock(&(colas->mutex_compactacion_activa));
  if (!colas->compactacion_activa)
  {
    desbloquear_cola_ready(&(colas->ready));
  }
  pthread_mutex_unlock(&(colas->mutex_compactacion_activa));
  restar_contador_hilos(colas);
  return NULL;
}

static void crear_hilo_desbloquear_cola_ready(t_colas* colas)
{
  pthread_t hilo;
  if (pthread_create(&hilo, NULL, hilo_desbloquear_cola_ready, colas) != 0)
  {
    logger_error(colas->logger,
                 "Error en la creación del hilo de desbloquear cola ready");
  }
  else
  {
    pthread_detach(hilo);
    logger_info(colas->logger,
                "Hilo de desbloquear cola ready iniciado exitosamente");
  }
}

static bool termino_compactacion(t_colas* colas)
{
  int op_code = -1;
  op_code = recibir_operacion(colas->socket_km->socket_km);
  switch (op_code)
  {
    case OP_NUEVO_MEMORY_STICK:
      free(recibir_string(colas->socket_km->socket_km));
      crear_hilo_rutina_des_suspension(colas);
      return termino_compactacion(colas);
    case OP_COMPACTACION_FINALIZADA:
      free(recibir_string(colas->socket_km->socket_km));
      return true;
    case OP_MEMORIA_CORRUPTA:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_MEMORIA_CORRUPTA, -1);
      return false;
    default:
      free(recibir_string(colas->socket_km->socket_km));
      cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
      return false;
  }
}

static void compactacion(t_colas* colas)
{
  if (!(enviar_string(OP_PUEDE_COMPACTAR, "Iniciar compactación",
                      colas->socket_km->socket_km)))
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    return;
  }
  logger_info(colas->logger, "## Inicio de compactacion");
  if (termino_compactacion(colas))
  {
    logger_info(colas->logger, "## Fin de compactacion");
  }
}

static bool avisar_nuevo_proceso(t_colas* colas, char* archivo_instrucciones,
                                 uint32_t pid)
{
  t_paquete* paquete = crear_paquete(OP_NUEVO_PROCESO);
  agregar_string_a_paquete(paquete, archivo_instrucciones);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));

  pthread_mutex_lock(&(colas->socket_km->mutex_socket));
  bool ret = enviar_paquete(paquete, colas->socket_km->socket_km);

  eliminar_paquete(paquete);

  if (!ret)
  {
    cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY,
                            colas->socket_km->socket_km);
    pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
    return false;
  }

  ret = false;
  bool seguir_operando = true;
  while (seguir_operando)
  {
    int op_code = recibir_operacion(colas->socket_km->socket_km);
    switch (op_code)
    {
      case OP_PROCESO_INICIADO:
        free(recibir_string(colas->socket_km->socket_km));
        ret = true;
        seguir_operando = false;
        break;
      case OP_MEMORIA_CORRUPTA:
        free(recibir_string(colas->socket_km->socket_km));
        cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                                MC_MEMORIA_CORRUPTA, -1);
        seguir_operando = false;
        break;
      case OP_NUEVO_MEMORY_STICK:
        free(recibir_string(colas->socket_km->socket_km));
        crear_hilo_rutina_des_suspension(colas);
        seguir_operando = true;
        break;
      default:
        free(recibir_string(colas->socket_km->socket_km));
        cerrar_kernel_scheduler(colas->socket_servidor, colas->logger,
                                MC_FALLO_CONEXION_KERNEL_MEMORY, -1);
        seguir_operando = false;
        break;
    }
  }

  pthread_mutex_unlock(&(colas->socket_km->mutex_socket));
  return ret;
}

#include <unistd.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/suspensor.h"

static void esperar_desbloqueo(t_datos_hilo_suspendido* datos)
{
  pthread_cond_wait(datos->esperar_proceso, &(datos->mutex_estado));
  if (datos->estado == EH_BLOQUEADO)
  {
    datos->estado == EH_EJECUTANDO;
  }
}

static t_pcb* obtener_proceso_bloqueado(t_datos_hilo_suspensor* datos)
{
  pthread_mutex_lock(&(datos->datos->colas->block.mutex_lista));
  t_pcb* proceso = NULL;
  if (!list_is_empty(datos->datos->colas->block.lista))
  {
    proceso = list_get(datos->datos->colas->block.lista, 0);
  }
  pthread_mutex_unlock(&(datos->datos->colas->block.mutex_lista));
  if (proceso == NULL)
  {
    datos->datos->estado = EH_ESPERANDO_PROCESO;
  }
  return proceso;
}

static void suspender_proceso(t_datos_hilo_suspensor* datos, t_pcb* proceso)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(proceso->mutex_estado));
  unsigned int tiempo_suspendido = proceso->tiempo_suspendido;
  pthread_mutex_unlock(&(proceso->mutex_estado));
  pthread_mutex_lock pthread_mutex_lock(&(datos->datos->mutex_estado));
}

static void esperar_proceso_bloqueado(t_datos_hilo_suspensor* datos)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(datos->datos->colas->block.mutex_lista));
  if (list_is_empty(datos->datos->colas->block.lista))
  {
    pthread_cond_wait(datos->esperar_proceso, &(datos->mutex_estado));
  }
  bool lista_vacia = list_is_empty(datos->datos->colas->block.lista);
  pthread_mutex_unlock(&(datos->datos->colas->block.mutex_lista));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
  if (!lista_vacia && datos->datos->estado == EH_ESPERANDO_PROCESO)
  {
    datos->datos->estado = EH_EJECUTANDO;
  }
}

static t_pcb* obtener_proceso_susp_ready(t_datos_hilo_des_suspensor* datos)
{
  pthread_mutex_lock(&(datos->datos->colas->susp_ready->mutex_lista));
  t_pcb* proceso = NULL;
  if (!list_is_empty(datos->datos->colas->susp_ready.lista))
  {
    proceso = list_get(datos->datos->colas->susp_ready.lista, 0);
  }
  pthread_mutex_unlock(&(datos->datos->colas->susp_ready->mutex_lista));
  if (proceso == NULL)
  {
    datos->datos->estado = EH_ESPERANDO_PROCESO;
  }
  return proceso;
}

static void des_suspender_proceso(t_datos_hilo_des_suspensor* datos,
                                  t_pcb* proceso)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
}

static void esperar_proceso_susp_ready(t_datos_hilo_des_suspensor* datos)
{
  pthread_mutex_unlock(&(datos->datos->mutex_estado));
  pthread_mutex_lock(&(datos->datos->colas->susp_block.mutex_lista));
  if (list_is_empty(datos->datos->colas->susp_block.lista))
  {
    pthread_cond_wait(datos->esperar_proceso, &(datos->mutex_estado));
  }
  bool lista_vacia = list_is_empty(datos->datos->colas->susp_block.lista);
  pthread_mutex_unlock(&(datos->datos->colas->susp_block.mutex_lista));
  pthread_mutex_lock(&(datos->datos->mutex_estado));
  if (!lista_vacia && datos->datos->estado == EH_ESPERANDO_PROCESO)
  {
    datos->datos->estado = EH_EJECUTANDO;
  }
}

static void* hilo_suspensor(void* datos_void)
{
  t_datos_hilo_suspensor* datos = (t_datos_hilo_suspensor*)datos_void;
  bool seguir_operando = true;
  while (seguir_operando)
  {
    pthread_mutex_lock(&(datos->datos->mutex_estado));
    switch (datos->datos->estado)
    {
      case EH_EJECUTANDO:
        t_pcb* proceso = obtener_proceso_bloqueado(datos);
        if (proceso != NULL)
        {
          suspender_proceso(datos, proceso);
        }
        break;
      case EH_ESPERANDO_PROCESO:
        esperar_proceso_bloqueado(datos);
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
  t_datos_hilo_des_suspensor* datos = (t_datos_hilo_des_suspensor*)datos_void;
  bool seguir_operando = true;
  while (seguir_operando)
  {
    pthread_mutex_lock(&(datos->datos->mutex_estado));
    switch (datos->datos->estado)
    {
      case EH_EJECUTANDO:
        t_pcb* proceso = obtener_proceso_susp_ready(datos);
        if (proceso != NULL)
        {
          des_suspender_proceso(datos, proceso);
        }
        break;
      case EH_ESPERANDO_PROCESO:
        esperar_proceso_susp_ready(datos);
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

static t_datos_hilo_suspendido* inicializar_datos_hilo_suspendido(
    t_colas* colas)
{
  t_datos_hilo_suspendido* datos = malloc(t_datos_hilo_suspendido);
  pthread_mutex_init(&(datos->mutex_estado), NULL);
  datos->estado = EH_EJECUTANDO;
  datos->mutex_suspender_des_suspender =
      &(colas->mutex_suspender_des_suspender);
  pthread_cond_init(&(datos->desbloquear), NULL);
  datos->colas = colas;
  datos->logger = colas->logger;
  return datos;
}

static t_datos_hilo_suspensor* inicializar_datos_hilo_suspensor(
    t_colas* colas, int suspension_timeout)
{
  t_datos_hilo_suspensor* datos = malloc(t_datos_hilo_suspensor);
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  datos->esperar_proceso = &(colas->block.cond_nuevo_proceso);
  datos->suspension_timeout = suspension_timeout;
  return datos;
}

static t_datos_hilo_des_suspensor* inicializar_datos_hilo_des_suspensor(
    t_colas* colas)
{
  t_datos_hilo_des_suspensor* datos = malloc(t_datos_hilo_des_suspensor);
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  datos->esperar_proceso = &(colas->susp_ready.cond_nuevo_proceso);
  return datos;
}

static void iniciar_hilo_suspensor(t_datos_hilo_suspensor* datos)
{
  if (pthread_create(&(datos->datos->hilo), NULL, hilo_suspensor, datos) != 0)
  {
    logger_error(datos->datos->logger,
                 "## Error en la creación del hilo suspensor");
  }
  else
  {
    logger_info(datos->datos->logger,
                "## Hilo suspensor iniciado exitosamente");
  }
}

static void iniciar_hilo_des_suspensor(t_datos_hilo_des_suspensor* datos)
{
  if (pthread_create(&(datos->datos->hilo), NULL, hilo_des_suspensor, datos) !=
      0)
  {
    logger_error(datos->datos->logger,
                 "## Error en la creación del hilo des-suspensor");
  }
  else
  {
    logger_info(datos->datos->logger,
                "## Hilo des-suspensor iniciado exitosamente");
  }
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
  pthread_join(datos->hilo);
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

void iniciar_hilos_suspendido(
    t_colas* colas, int suspension_timeout,
    t_datos_hilo_suspensor** datos_hilo_suspensor,
    t_datos_hilo_des_suspensor** datos_hilo_des_suspensor)
{
  *datos_hilo_suspensor =
      inicializar_datos_hilo_suspensor(colas, suspension_timeout);
  *datos_hilo_des_suspensor = inicializar_datos_hilo_des_suspensor(colas);
  iniciar_hilo_suspensor(*datos_hilo_suspensor);
  iniciar_hilo_des_suspensor(*datos_hilo_des_suspensor);
}

void terminar_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor)
{
  terminar_hilo_suspendido(datos_hilo_suspensor->datos);
  terminar_hilo_suspendido(datos_hilo_des_suspensor->datos);
  esperar_hilo_suspendido(datos_hilo_suspensor->datos);
  esperar_hilo_suspendido(datos_hilo_des_suspensor->datos);
}

void destruir_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor)
{
  destruir_hilo_suspensor(datos_hilo_suspensor);
  destruir_hilo_des_suspensor(datos_hilo_des_suspensor);
}

void bloquear_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor)
{
  bloquear_hilo_suspendido(datos_hilo_suspensor->datos);
  bloquear_hilo_suspendido(datos_hilo_des_suspensor->datos);
}

void desbloquear_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor)
{
  desbloquear_hilo_suspendido(datos_hilo_suspensor->datos);
  desbloquear_hilo_suspendido(datos_hilo_des_suspensor->datos);
}

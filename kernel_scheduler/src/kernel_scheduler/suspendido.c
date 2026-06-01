#include "kernel_scheduler/suspensor.h"

static t_datos_hilo_suspensor* inicializar_datos_hilo_suspendido(t_colas* colas)
{
  t_datos_hilo_suspendido* datos = malloc(t_datos_hilo_suspendido);
  pthread_mutex_init(&(datos->mutex_estado), NULL);
  datos->estado = EH_ESPERANDO_PROCESO;
  datos->mutex_suspender_des_suspender =
      &(colas->mutex_suspender_des_suspender);

  return datos_hilo_suspensor;
}

static t_datos_hilo_suspensor* inicializar_datos_hilo_suspensor(
    t_colas* colas, int suspension_timeout)
{
  t_datos_hilo_suspensor* datos = malloc(t_datos_hilo_suspensor);
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  datos->suspension_timeout = suspension_timeout;
  return datos;
}

static t_datos_hilo_des_suspensor* inicializar_datos_hilo_des_suspensor(
    t_colas* colas)
{
  t_datos_hilo_des_suspensor* datos = malloc(t_datos_hilo_des_suspensor);
  datos->datos = inicializar_datos_hilo_suspendido(colas);
  return datos;
}

static void terminar_hilo_suspensor(
    t_datos_hilo_suspensor* datos_hilo_suspensor);

static void terminar_hilo_des_suspensor(
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

static void esperar_hilo_suspensor(
    t_datos_hilo_suspensor* datos_hilo_suspensor);

static void esperar_hilo_des_suspensor(
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

static void destruir_hilo_suspensor(
    t_datos_hilo_suspensor* datos_hilo_suspensor);

static void destruir_hilo_des_suspensor(
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

static void bloquear_hilo_suspensor(
    t_datos_hilo_suspensor* datos_hilo_suspensor);

static void bloquear_hilo_des_suspensor(
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

static void desbloquear_hilo_suspensor(
    t_datos_hilo_suspensor* datos_hilo_suspensor);

static void desbloquear_hilo_des_suspensor(
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor);

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
  terminar_hilo_suspensor(datos_hilo_suspensor);
  terminar_hilo_des_suspensor(datos_hilo_des_suspensor);
  esperar_hilo_suspensor(datos_hilo_suspensor);
  esperar_hilo_des_suspensor(datos_hilo_des_suspensor);
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
  bloquear_hilo_suspensor(datos_hilo_suspensor);
  bloquear_hilo_des_suspensor(datos_hilo_des_suspensor);
}

void desbloquear_hilos_suspendido(
    t_datos_hilo_suspensor* datos_hilo_suspensor,
    t_datos_hilo_des_suspensor* datos_hilo_des_suspensor)
{
  desbloquear_hilo_suspensor(datos_hilo_suspensor);
  desbloquear_hilo_des_suspensor(datos_hilo_des_suspensor);
}

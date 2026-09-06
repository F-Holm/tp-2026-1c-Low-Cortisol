#include "kernel_scheduler/kernel_memory.h"

#include <pthread.h>
#include <stdbool.h>

#include "utils/msg.h"

static int conectar_kernel_memory(char* ip, char* puerto, t_log* logger);
static bool handshake_kernel_memory(int socket_km, t_log* logger);
static void* hilo_verificar_conexion_kernel_memory(void* arg);

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_log* logger)
{
  int socket_km = conectar_kernel_memory(ip, puerto, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake_kernel_memory(socket_km, logger))
    return -1;

  return socket_km;
}

bool avisar_terminar_proceso(t_socket_kernel_memory* socket_km, uint32_t pid,
                             int socket_servidor, t_log* logger)
{
  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool ret =
      send_buffer(OP_END_PROCESS, &pid, sizeof(uint32_t), socket_km->socket_km);
  if (!ret)
  {
    cerrar_kernel_scheduler(socket_servidor, logger,
                            MC_ERROR_ENVIO_KERNEL_MEMORY, socket_km->socket_km);
  }
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  return ret;
}

t_datos_hilo_verificar_conexion* iniciar_hilo_verificar_conexion_kernel_memory(
    int socket_servidor, t_log* logger, t_socket_kernel_memory* socket_km)
{
  t_datos_hilo_verificar_conexion* datos =
      malloc(sizeof(t_datos_hilo_verificar_conexion));
  datos->socket_servidor = socket_servidor;
  datos->logger = logger;
  datos->socket_km = socket_km;
  datos->cerrar = false;
  pthread_mutex_init(&(datos->mutex_cerrar), NULL);

  if (pthread_create(&(datos->hilo), NULL,
                     hilo_verificar_conexion_kernel_memory, datos) != 0)
  {
    log_error(logger,
              "Error en la creación del hilo verificador de la conexión con "
              "Kernel Memory");
  }
  return datos;
}

void destruir_hilo_verificar_conexion_kernel_memory(
    t_datos_hilo_verificar_conexion* datos)
{
  pthread_mutex_lock(&(datos->mutex_cerrar));
  datos->cerrar = true;
  pthread_mutex_unlock(&(datos->mutex_cerrar));
  pthread_join(datos->hilo, NULL);
  pthread_mutex_destroy(&(datos->mutex_cerrar));
  free(datos);
}

static int conectar_kernel_memory(char* ip, char* puerto, t_log* logger)
{
  int socket_km = create_connection(ip, puerto);
  if (socket_km <= 0)
  {
    log_error(logger, "Error de conexión con Kernel Memory");
    return -1;
  }
  log_info(logger, "## Conectado a Kernel Memory");
  return socket_km;
}

static bool handshake_kernel_memory(int socket_km, t_log* logger)
{
  if (!send_handshake(MID_KERNEL_SCHEDULER, socket_km))
  {
    log_error(logger, "Error en el envio del Handshake con Kernel Memory");
    return false;
  }
  if (receive_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    log_error(logger, "Error en la recepción del Handshake con Kernel Memory");
    return false;
  }
  log_info(logger, "Handshake exitoso con Kernel Memory");
  return true;
}

static void* hilo_verificar_conexion_kernel_memory(void* args)
{
  t_datos_hilo_verificar_conexion* datos =
      (t_datos_hilo_verificar_conexion*)args;
  bool seguir_operando = true;
  while (seguir_operando)
  {
    usleep(500000);
    pthread_mutex_lock(&(datos->socket_km->mutex_socket));
    seguir_operando = send_string(OP_KERNEL_MEMORY_RUNNING,
                                  "¿El Kernel Memory sigue conectado?",
                                  datos->socket_km->socket_km);
    if (!seguir_operando)
    {
      cerrar_kernel_scheduler(datos->socket_servidor, datos->logger,
                              MC_ERROR_ENVIO_KERNEL_MEMORY,
                              datos->socket_km->socket_km);
    }
    else
    {
      pthread_mutex_lock(&(datos->mutex_cerrar));
      seguir_operando = !datos->cerrar;
      pthread_mutex_unlock(&(datos->mutex_cerrar));
    }
    pthread_mutex_unlock(&(datos->socket_km->mutex_socket));
  }
  return NULL;
}

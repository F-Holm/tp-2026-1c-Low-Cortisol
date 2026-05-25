#include "kernel_scheduler/kernel_memory.h"

#include <pthread.h>
#include <stdbool.h>

#include "utils/client.h"
#include "utils/msg.h"

static int conectar_kernel_memory(char* ip, char* puerto, t_logger* logger)
{
  int socket_km = crear_conexion(ip, puerto);
  if (socket_km <= 0)
  {
    logger_error(logger, "## Error de conexión con Kernel Memory");
    return -1;
  }
  logger_info(logger, "## Conectado a Kernel Memory");
  return socket_km;
}

static bool handshake_kernel_memory(int socket_km, t_logger* logger)
{
  if (!enviar_handshake(MID_KERNEL_SCHEDULER, socket_km))
  {
    logger_error(logger,
                 "## Error en el envio del Handshake con Kernel Memory");
    return false;
  }
  if (recibir_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    logger_error(logger,
                 "## Error en la recepción del Handshake con Kernel Memory");
    return false;
  }
  logger_info(logger, "## Handshake exitoso con Kernel Memory");
  return true;
}

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_logger* logger)
{
  int socket_km = conectar_kernel_memory(ip, puerto, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake_kernel_memory(socket_km, logger))
    return -1;

  return socket_km;
}

bool avisar_nuevo_proceso(t_socket_kernel_memory* socket_km,
                          char* archivo_instrucciones, uint32_t pid)
{
  t_paquete* paquete = crear_paquete(OP_NUEVO_PROCESO);
  agregar_string_a_paquete(paquete, archivo_instrucciones);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));

  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool ret = enviar_paquete(paquete, socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));

  eliminar_paquete(paquete);
  return ret;
}

bool avisar_terminar_proceso(t_socket_kernel_memory* socket_km, uint32_t pid)
{
  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool ret = enviar_buffer(OP_TERMINAR_PROCESO, &pid, sizeof(uint32_t),
                           socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  return ret;
}

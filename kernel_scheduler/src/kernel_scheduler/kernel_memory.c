#include "kernel_scheduler/kernel_memory.h"

#include <stdbool.h>

#include "utils/client.h"
#include "utils/msg.h"

int conectar_kernel_memory(char* ip, char* puerto, t_log* logger)
{
  int socket_km = crear_conexion(ip, puerto);
  if (socket_km <= 0)
  {
    log_error(logger, "## Error de conexión con Kernel Memory");
    return -1;
  }
  log_info(logger, "## Conectado a Kernel Memory");
  return socket_km;
}

bool handshake_kernel_memory(int socket_km, t_log* logger)
{
  if (!enviar_handshake(MID_KERNEL_SCHEDULER, socket_km))
  {
    log_error(logger, "## Error en el envio del Handshake con Kernel Memory");
    return false;
  }
  if (recibir_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    log_error(logger,
              "## Error en la recepción del Handshake con Kernel Memory");
    return false;
  }
  log_info(logger, "## Handshake exitoso con Kernel Memory");
  return true;
}

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_log* logger)
{
  int socket_km = conectar_kernel_memory(ip, puerto, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake_kernel_memory(socket_km, logger))
    return -1;

  return socket_km;
}

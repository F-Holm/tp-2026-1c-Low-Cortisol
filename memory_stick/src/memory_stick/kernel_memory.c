#include "memory_stick/kernel_memory.h"

#include <stdio.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

bool handshake_km(int socket_km, t_log* logger)
{
  if (!enviar_handshake(MID_MEMORY_STICK, socket_km))
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

bool enviar_tamanio(int socket_km, char* tamanio, t_log* logger)
{
  if (!enviar_string(OP_TAMANIO_MEMORIA, tamanio, socket_km))
  {
    log_error(logger, "## Error en el envio de tamaño");
    return false;
  }
  log_info(logger, "## Envio de tamaño exitoso");
  return true;
}

int iniciar_conexion_km(char* ip, char* puerto, char* tamanio, t_log* logger)
{
  int socket_km = conectar_km(ip, puerto, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake_km(socket_km, logger))
    return -1;

  if (!enviar_tamanio(socket_km, tamanio, logger))
    return -1;

  return socket_km;
}

int conectar_km(char* ip, char* puerto, t_log* logger)
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

bool enviar_puerto_server_ms_km(int socket, uint16_t puerto)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%u", puerto);
  return enviar_string(OP_PUERTO, buffer, socket);
}

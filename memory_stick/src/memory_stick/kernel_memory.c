#include "memory_stick/kernel_memory.h"

#include <stdio.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

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

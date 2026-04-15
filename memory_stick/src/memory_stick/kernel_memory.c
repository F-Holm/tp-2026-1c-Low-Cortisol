#include "memory_stick/kernel_memory.h"

#include <stdio.h>

#include "utils/msg.h"

int conectar_km(char* ip, char* puerto)
{
  return crear_conexion(ip, puerto);
}

void enviar_puerto_server_ms_km(int socket, uint16_t puerto)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%u", puerto);
  enviar_string(OP_PUERTO, buffer, socket);
}

#include "memory_stick/kernel_memory.h"

#include <stdio.h>

int conectar_km(char* ip, char* puerto)
{
  return crear_conexion(ip, puerto);
}

bool handshake_km(int socket)
{
  enviar_mensaje("memory_stick", socket);
  char* ack = recibir_mensaje(socket);
  if (socket > -1 && ack != NULL && strcmp(ack, "kernel_memory"))
  {
    free(ack);
    return true;
  }
  return false;
}

void enviar_puerto_server_ms_km(int socket, uint16_t puerto)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%u", puerto);
  enviar_mensaje(buffer, socket);
}

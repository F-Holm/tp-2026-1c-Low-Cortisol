#include "memory_stick/kernel_memory.h"

int conectar_kernel_memory(char* ip, char* puerto)
{
  return crear_conexion(ip, puerto);
}

void desconectar_kernel_memory(int socket)
{
  close(socket);
}

bool handshake(int socket)
{
  enviar_mensaje("memory_stick", socket);
  char* ack = recibir_mensaje(socket);
  if (socket > -1 && ack != NULL && strcmp(ack, "kernel_memory")) {
    free(ack);
    return true;
  }
  return false;
}

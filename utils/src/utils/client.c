#include "utils/client.h"

#include "utils/msg.h"

int crear_conexion(char* ip, char* puerto)
{
  struct addrinfo hints;
  struct addrinfo* server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(ip, puerto, &hints, &server_info);

  // Ahora vamos a crear el socket.
  int socket = 0;
  socket = socket(server_info->ai_family, server_info->ai_socktype,
                  server_info->ai_protocol);

  // Ahora que tenemos el socket, vamos a conectarlo
  connect(socket, server_info->ai_addr, server_info->ai_addrlen);

  freeaddrinfo(server_info);

  return socket;
}

void liberar_conexion(int socket)
{
  close(socket);
}

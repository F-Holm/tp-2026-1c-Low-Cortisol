#include "utils/server.h"

int iniciar_servidor(char* puerto)
{
  int socket_servidor;

  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, puerto, &hints, &servinfo);

  // Creamos el socket de escucha del servidor
  socket_servidor =
      socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);
  // Asociamos el socket a un puerto
  bind(socket_servidor, servinfo->ai_addr, servinfo->ai_addrlen);
  // Escuchamos las conexiones entrantes
  listen(socket_servidor, SOMAXCONN);

  freeaddrinfo(servinfo);

  return socket_servidor;
}

int esperar_cliente(int socket_servidor)
{
  // Aceptamos un nuevo cliente
  // Si se cerró el servidor, socket_cliente == -1
  int socket_cliente = accept(socket_servidor, NULL, NULL);
  return socket_cliente;
}

#include "utils/client.h"

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
  int fd_socket = 0;
  fd_socket = socket(server_info->ai_family, server_info->ai_socktype,
                     server_info->ai_protocol);

  // Ahora que tenemos el socket, vamos a conectarlo
  connect(fd_socket, server_info->ai_addr, server_info->ai_addrlen);

  freeaddrinfo(server_info);

  return fd_socket;
}

void liberar_conexion(int socket_fd)
{
  close(socket_fd);
}

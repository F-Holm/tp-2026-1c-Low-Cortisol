#include "utils/client.h"

int crear_conexion(char* ip, char* puerto)
{
  struct addrinfo hints;
  struct addrinfo* server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int err = getaddrinfo(ip, puerto, &hints, &server_info);
  if (err != 0)
    return -1;

  int fd_socket = socket(server_info->ai_family, server_info->ai_socktype,
                         server_info->ai_protocol);
  if (fd_socket == -1)
  {
    freeaddrinfo(server_info);
    return -1;
  }

  err = connect(fd_socket, server_info->ai_addr, server_info->ai_addrlen);
  if (err == -1)
  {
    freeaddrinfo(server_info);
    liberar_conexion(fd_socket);
    return -1;
  }

  freeaddrinfo(server_info);

  return fd_socket;
}

void liberar_conexion(int socket_fd)
{
  close(socket_fd);
}

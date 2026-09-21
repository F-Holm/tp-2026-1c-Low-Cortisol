#include "utils/sockets_linux.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static const t_socket_handle INVALID_HANDLE = -1;

t_socket_handle socket_handle_invalid(void)
{
  return INVALID_HANDLE;
}

bool socket_handle_is_valid(t_socket_handle handle)
{
  return handle != INVALID_HANDLE;
}

t_socket_handle socket_handle_connect(char* ip, char* port)
{
  struct addrinfo hints;
  struct addrinfo* server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(ip, port, &hints, &server_info) != 0)
    return INVALID_HANDLE;

  t_socket_handle handle =
      socket(server_info->ai_family, server_info->ai_socktype,
             server_info->ai_protocol);
  if (!socket_handle_is_valid(handle))
  {
    freeaddrinfo(server_info);
    return INVALID_HANDLE;
  }

  if (connect(handle, server_info->ai_addr, server_info->ai_addrlen) == -1)
  {
    freeaddrinfo(server_info);
    close(handle);
    return INVALID_HANDLE;
  }

  freeaddrinfo(server_info);
  return handle;
}

t_socket_handle socket_handle_listen(char* port)
{
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, port, &hints, &servinfo);

  t_socket_handle handle =
      socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

  // Make the socket reusable; remove if it causes issues.
  int opt = 1;
  setsockopt(handle, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  bind(handle, servinfo->ai_addr, servinfo->ai_addrlen);
  listen(handle, SOMAXCONN);

  freeaddrinfo(servinfo);
  return handle;
}

t_socket_handle socket_handle_accept(t_socket_handle listener)
{
  return accept(listener, NULL, NULL);
}

bool socket_handle_send(t_socket_handle handle, const void* data, int size)
{
  return send(handle, data, size, MSG_NOSIGNAL) > 0;
}

bool socket_handle_receive(t_socket_handle handle, void* data, int size)
{
  return recv(handle, data, size, MSG_WAITALL) > 0;
}

int socket_handle_get_local_port(t_socket_handle handle)
{
  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  if (getsockname(handle, (struct sockaddr*)&address, &length) != 0)
    return -1;

  return ntohs(address.sin_port);
}

void socket_handle_shutdown(t_socket_handle handle, bool disable_read,
                            bool disable_write)
{
  if (disable_read && disable_write)
    shutdown(handle, SHUT_RDWR);
  else if (disable_read)
    shutdown(handle, SHUT_RD);
  else if (disable_write)
    shutdown(handle, SHUT_WR);
}

void socket_handle_close(t_socket_handle handle)
{
  close(handle);
}

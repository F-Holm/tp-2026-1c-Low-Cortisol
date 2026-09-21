#include "utils/os.h"

#ifdef OS_LINUX

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/sockets.h"

static const t_socket_handle INVALID_HANDLE = -1;

static bool socket_handle_is_valid(t_socket_handle handle);
static t_socket* wrap_handle(t_socket_handle handle, bool with_mutex);
static t_socket* create_client(char* ip, char* port, bool with_mutex);
static t_socket* create_server(char* port, bool with_mutex);

t_socket* socket_create(t_socket_kind kind, char* ip, char* port,
                        bool with_mutex)
{
  if (kind == SOCKET_KIND_CLIENT)
    return create_client(ip, port, with_mutex);
  return create_server(port, with_mutex);
}

t_socket* socket_accept(t_socket* listener, bool with_mutex)
{
  return wrap_handle(accept(listener->handle, NULL, NULL), with_mutex);
}

bool socket_send(t_socket* socket, const void* data, int size)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return false;
  return send(socket->handle, data, size, MSG_NOSIGNAL) > 0;
}

bool socket_receive(t_socket* socket, void* data, int size)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return false;
  return recv(socket->handle, data, size, MSG_WAITALL) > 0;
}

int socket_get_local_port(t_socket* socket)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return -1;

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  if (getsockname(socket->handle, (struct sockaddr*)&address, &length) != 0)
    return -1;

  return ntohs(address.sin_port);
}

void socket_shutdown(t_socket* socket, t_socket_shutdown_mode mode)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return;

  static const int HOW[] = {SHUT_RD, SHUT_WR, SHUT_RDWR};
  shutdown(socket->handle, HOW[mode]);
}

void socket_close(t_socket* socket)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return;
  close(socket->handle);
  socket->handle = INVALID_HANDLE;
}

void socket_destroy(t_socket* socket)
{
  if (socket == NULL)
    return;
  socket_close(socket);
  mtx_destroy(&socket->mutex);
  free(socket);
}

void socket_mutex_lock(t_socket* socket)
{
  if (socket == NULL || !socket->has_mutex)
    return;
  mtx_lock(&socket->mutex);
}

void socket_mutex_unlock(t_socket* socket)
{
  if (socket == NULL || !socket->has_mutex)
    return;
  mtx_unlock(&socket->mutex);
}

static bool socket_handle_is_valid(t_socket_handle handle)
{
  return handle != INVALID_HANDLE;
}

static t_socket* wrap_handle(t_socket_handle handle, bool with_mutex)
{
  if (!socket_handle_is_valid(handle))
    return NULL;

  t_socket* socket = malloc(sizeof(t_socket));
  socket->handle = handle;
  socket->has_mutex = with_mutex;
  mtx_init(&socket->mutex);
  return socket;
}

static t_socket* create_client(char* ip, char* port, bool with_mutex)
{
  struct addrinfo hints;
  struct addrinfo* server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(ip, port, &hints, &server_info) != 0)
    return NULL;

  t_socket_handle handle =
      socket(server_info->ai_family, server_info->ai_socktype,
             server_info->ai_protocol);
  if (!socket_handle_is_valid(handle))
  {
    freeaddrinfo(server_info);
    return NULL;
  }

  if (connect(handle, server_info->ai_addr, server_info->ai_addrlen) == -1)
  {
    freeaddrinfo(server_info);
    close(handle);
    return NULL;
  }

  freeaddrinfo(server_info);
  return wrap_handle(handle, with_mutex);
}

static t_socket* create_server(char* port, bool with_mutex)
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
  return wrap_handle(handle, with_mutex);
}

#endif  // OS_LINUX

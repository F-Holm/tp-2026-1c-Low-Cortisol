#include "utils/sockets.h"

#include <stdlib.h>

#include "utils/sockets_linux.h"
// -- future Windows support: swap for "utils/sockets_windows.h" behind an
//    #ifdef _WIN32. Both headers expose the same socket_handle_*() names, so
//    nothing else in this file needs to change.

// The glue between a raw platform handle and the public t_socket type: this
// is the only place that allocates a t_socket or its mutex, so it is written
// once and shared by every platform backend.
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

t_socket* socket_create(t_socket_kind kind, char* ip, char* port,
                        bool with_mutex)
{
  t_socket_handle handle = kind == SOCKET_KIND_CLIENT
                               ? socket_handle_connect(ip, port)
                               : socket_handle_listen(port);
  return wrap_handle(handle, with_mutex);
}

t_socket* socket_accept(t_socket* listener, bool with_mutex)
{
  return wrap_handle(socket_handle_accept(listener->handle), with_mutex);
}

bool socket_send(t_socket* socket, const void* data, int size)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return false;
  return socket_handle_send(socket->handle, data, size);
}

bool socket_receive(t_socket* socket, void* data, int size)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return false;
  return socket_handle_receive(socket->handle, data, size);
}

int socket_get_local_port(t_socket* socket)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return -1;
  return socket_handle_get_local_port(socket->handle);
}

void socket_shutdown(t_socket* socket, t_socket_shutdown_mode mode)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return;

  bool disable_read =
      mode == SOCKET_SHUTDOWN_READ || mode == SOCKET_SHUTDOWN_BOTH;
  bool disable_write =
      mode == SOCKET_SHUTDOWN_WRITE || mode == SOCKET_SHUTDOWN_BOTH;
  socket_handle_shutdown(socket->handle, disable_read, disable_write);
}

void socket_close(t_socket* socket)
{
  if (socket == NULL || !socket_handle_is_valid(socket->handle))
    return;
  socket_handle_close(socket->handle);
  socket->handle = socket_handle_invalid();
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

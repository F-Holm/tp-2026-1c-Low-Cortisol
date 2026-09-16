#include "kernel_scheduler/domain/kernel_memory_socket.h"

#include <stdlib.h>

t_kernel_memory_socket* init_socket_kernel_memory(int km_socket)
{
  t_kernel_memory_socket* km_socket_mutex =
      malloc(sizeof(t_kernel_memory_socket));
  km_socket_mutex->km_socket = km_socket;
  pthread_mutex_init(&(km_socket_mutex->socket_mutex), NULL);
  return km_socket_mutex;
}

void destroy_kernel_memory(t_kernel_memory_socket* km_socket)
{
  pthread_mutex_destroy(&(km_socket->socket_mutex));
  free(km_socket);
}

#pragma once

#include <pthread.h>

// The Kernel Scheduler holds a single connection to Kernel Memory. Its mutex
// serialises whole request/response exchanges over that socket so concurrent
// scheduler threads never interleave their protocol messages.
typedef struct
{
  int km_socket;
  pthread_mutex_t socket_mutex;
} t_kernel_memory_socket;

t_kernel_memory_socket* init_socket_kernel_memory(int km_socket);
void destroy_kernel_memory(t_kernel_memory_socket* km_socket);

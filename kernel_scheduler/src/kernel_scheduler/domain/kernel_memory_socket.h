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

/** @brief Wraps @p km_socket with its serializing mutex. */
t_kernel_memory_socket* init_socket_kernel_memory(int km_socket);

/** @brief Destroys the mutex and frees @p km_socket (does not close the fd). */
void destroy_kernel_memory(t_kernel_memory_socket* km_socket);

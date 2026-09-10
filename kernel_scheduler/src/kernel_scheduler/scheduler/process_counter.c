#include "kernel_scheduler/scheduler/process_counter.h"

#include <stdbool.h>
#include <stdlib.h>

#include "kernel_scheduler/shutdown.h"

t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket)
{
  t_process_counter* counter = malloc(sizeof(t_process_counter));
  atomic_init(&(counter->active_process_count), 0);
  counter->server_socket = server_socket;
  counter->logger = logger;
  counter->km_socket = km_socket;
  return counter;
}

void aumentar_counter_processes(t_process_counter* counter)
{
  atomic_fetch_add(&(counter->active_process_count), 1);
}

void disminuir_counter_processes(t_process_counter* counter)
{
  bool is_last = atomic_fetch_sub(&(counter->active_process_count), 1) == 1;

  if (is_last)
  {
    pthread_mutex_lock(&(counter->km_socket->socket_mutex));
    close_kernel_scheduler(counter->server_socket, counter->logger,
                           SR_NO_PROCESSES, counter->km_socket->km_socket);
    pthread_mutex_unlock(&(counter->km_socket->socket_mutex));
  }
}

void destroy_counter_processes(t_process_counter* counter)
{
  free(counter);
}

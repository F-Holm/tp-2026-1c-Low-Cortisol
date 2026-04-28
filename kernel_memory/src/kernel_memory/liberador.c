#include "kernel_memory/liberador.h"

#include "kernel_memory/configurador.h"
#include <stdlib.h>

void liberar_datos_cpu(t_datos_cpu* datos_cpu)
{
  if (datos_cpu == NULL)
    return;
  terminar_comunicacion(datos_cpu->socket_cpu);
  free(datos_cpu);
}

void liberar_datos_stick(t_datos_stick* datos_stick)
{
  if (datos_stick == NULL)
    return;
  terminar_comunicacion(datos_stick->socket_stick);
  free(datos_stick);
}

void liberar_datos_swap(t_datos_swap* datos_swap)
{
  if (datos_swap == NULL)
    return;
  terminar_comunicacion(datos_swap->socket_swap);
  free(datos_swap);
}

void liberar_datos_scheduler(t_datos_scheduler* datos_scheduler)
{
  if (datos_scheduler == NULL)
    return;
  terminar_comunicacion(datos_scheduler->socket_scheduler);
  free(datos_scheduler);
}

void liberar_datos_kernel_mem(t_datos_kernel_mem* datos_kernel)
{
  if (datos_kernel == NULL)
    return;

  // Liberar mutex
  pthread_mutex_destroy(&datos_kernel->mutex_lista_sockets);

  // Liberar listas (solo la estructura, no los elementos)
  if (datos_kernel->sticks_conectados != NULL)
    list_destroy(datos_kernel->sticks_conectados);
  if (datos_kernel->cpus_conectados != NULL)
    list_destroy(datos_kernel->cpus_conectados);

  // Cerrar socket principal
  if (datos_kernel->socket_kernel_memory > 0)
    terminar_comunicacion(datos_kernel->socket_kernel_memory);

  free(datos_kernel);
}
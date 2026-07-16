#include "kernel_memory/liberador.h"

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
  shutdown(datos_scheduler->socket_kernel_memory, SHUT_RDWR);
  free(datos_scheduler);
}

void liberar_datos_kernel_mem(t_datos_kernel_mem* datos_kernel)
{
  logger_info(datos_kernel->logger,
              "Liberando datos de kernel memory y finalizando programa.");
  if (datos_kernel == NULL)
    return;

  // Liberar mutex
  pthread_mutex_destroy(datos_kernel->mutex_lista_sockets);
  pthread_mutex_destroy(datos_kernel->mutex_procesos);
  free(datos_kernel->mutex_lista_sockets);
  free(datos_kernel->mutex_procesos);
  // Liberar listas (solo la estructura, no los elementos)
  if (datos_kernel->sticks_conectados != NULL)
  {
    for (int i = 0; i < list_size(datos_kernel->sticks_conectados); i++)
    {
      t_datos_stick* stick =
          (t_datos_stick*)list_get(datos_kernel->sticks_conectados, i);
      liberar_datos_stick(stick);
    }
    list_destroy(datos_kernel->sticks_conectados);
  }
  if (datos_kernel->cpus_conectados != NULL)
  {
    for (int i = 0; i < list_size(datos_kernel->cpus_conectados); i++)
    {
      t_datos_cpu* cpu =
          (t_datos_cpu*)list_get(datos_kernel->cpus_conectados, i);
      liberar_datos_cpu(cpu);
    }
    list_destroy(datos_kernel->cpus_conectados);
  }
  // falta liberar memoria principal y datos swap y procesos si es necesario

  // Cerrar socket principal
  if (datos_kernel->socket_kernel_memory > 0)
  {
    terminar_comunicacion(datos_kernel->socket_kernel_memory);
  }

  free(datos_kernel);
}

void liberar_proceso(t_proceso* proceso)
{
  for (int i = 0; i < proceso->cant_instrucciones; i++)
  {
    free(proceso->instrucciones[i]);
  }
  free(proceso->instrucciones);
  list_destroy_and_destroy_elements(proceso->segmentos, free);
  free(proceso);
}
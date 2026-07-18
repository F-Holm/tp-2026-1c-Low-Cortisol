#include "kernel_memory/liberador.h"

void liberar_datos_cpu(t_datos_cpu* datos_cpu)
{
  cerrar_cpu(datos_cpu);
  free(datos_cpu);
}

void cerrar_cpu(t_datos_cpu* cpu)
{
  if (cpu->socket_cpu != -1)
  {
    terminar_comunicacion(cpu->socket_cpu);
  }
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
  if (datos_swap->lista_bloques != NULL)
  {
    list_destroy_and_destroy_elements(datos_swap->lista_bloques, free);
  }
  if (datos_swap->socket_swap > 0)
  {
    terminar_comunicacion(datos_swap->socket_swap);
  }
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
  if (datos_kernel->cpus_conectados != NULL)
  {
    for (int i = 0; i < list_size(datos_kernel->cpus_conectados); i++)
    {
      t_datos_cpu* cpu = list_get(datos_kernel->cpus_conectados, i);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
    }
  }
  if (datos_kernel->socket_scheduler != -1)
  {
    shutdown(datos_kernel->socket_scheduler, SHUT_RDWR);
  }
  pthread_mutex_lock(datos_kernel->mutex_hilos_activos);
  while (datos_kernel->hilos_activos > 0)
  {
    pthread_cond_wait(datos_kernel->cond_hilos_activos,
                      datos_kernel->mutex_hilos_activos);
  }
  pthread_mutex_unlock(datos_kernel->mutex_hilos_activos);

  // Liberar mutex
  pthread_mutex_destroy(datos_kernel->mutex_lista_sockets);
  pthread_mutex_destroy(datos_kernel->mutex_procesos);
  free(datos_kernel->mutex_lista_sockets);
  free(datos_kernel->mutex_procesos);

  pthread_mutex_destroy(datos_kernel->mutex_hilos_activos);
  pthread_cond_destroy(datos_kernel->cond_hilos_activos);
  free(datos_kernel->mutex_hilos_activos);
  free(datos_kernel->cond_hilos_activos);
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
    /*for (int i = 0; i < list_size(datos_kernel->cpus_conectados); i++)
    {
      t_datos_cpu* cpu =
          (t_datos_cpu*)list_get(datos_kernel->cpus_conectados, i);
      //liberar_datos_cpu(cpu);
      list_remove(datos_kernel->cpus_conectados, cpu);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
    }*/
    while (!list_is_empty(datos_kernel->cpus_conectados))
    {
      t_datos_cpu* cpu = list_remove(datos_kernel->cpus_conectados, 0);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
      free(cpu);
    }
    list_destroy(datos_kernel->cpus_conectados);
  }
  if (datos_kernel->datos_swap != NULL)
  {
    liberar_datos_swap(datos_kernel->datos_swap);
  }
  if (datos_kernel->memoria_principal != NULL)
  {
    liberar_memoria_principal(datos_kernel->memoria_principal);
  }
  if (datos_kernel->procesos != NULL)
  {
    for (int i = 0; i < list_size(datos_kernel->procesos); i++)
    {
      t_proceso* p = list_get(datos_kernel->procesos, i);
      liberar_proceso(p);
    }
    list_destroy(datos_kernel->procesos);
  }
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

void liberar_memoria_principal(t_memoria_principal* memoria)
{
  if (memoria == NULL)
    return;
  pthread_mutex_destroy(memoria->mutex_memoria_principal);
  free(memoria->mutex_memoria_principal);
  if (memoria->segmentos != NULL)
    list_destroy_and_destroy_elements(memoria->segmentos, free);
  if (memoria->huecos != NULL)
    list_destroy_and_destroy_elements(memoria->huecos, free);
  free(memoria);
}
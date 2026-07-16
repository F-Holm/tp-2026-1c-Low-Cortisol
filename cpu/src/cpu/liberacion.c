#include "cpu/liberacion.h"

#include <commons/log.h>
#include <stdio.h>

#include "cpu/cpu.h"

void iterator_close_socket(void* value)
{
  close(*((int*)value));
  free(value);
}

void destruir_instruccion(t_instruccion* instrucion)
{
  free(instrucion->nombre);
  for (int i = 0; i < instrucion->cantidad_parametros; i++)
    free(instrucion->parametros[i]);
  free(instrucion);
}

void destruir_memory_stick(void* value)
{
  t_memory_stick_info* stick = (t_memory_stick_info*)value;
  if (stick->socket_MS > 0)
    close(stick->socket_MS);
  free(stick);
}

void cerrar_modulo(t_cpu* cpu)
{
  if (cpu->memory_sticks != NULL)
    list_destroy_and_destroy_elements(cpu->memory_sticks,
                                      destruir_memory_stick);

  if (cpu->socket_kernel_memory > 0)
    close(cpu->socket_kernel_memory);

  if (cpu->socket_kernel_scheduler > 0)
    close(cpu->socket_kernel_scheduler);

  if (cpu->config != NULL)
    config_destroy(cpu->config);

  if (cpu->handlers != NULL)
    dictionary_destroy(cpu->handlers);

  if (cpu->logger != NULL)
  {
    log_info(cpu->logger, "MODULO CERRADO");
    log_destroy(cpu->logger);
  }
  free(cpu);
}

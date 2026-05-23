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

void cerrar_modulo(t_cpu* cpu)
{
  if (cpu->socket_kernel_memory > 0)
    close(cpu->socket_kernel_memory);
  cpu->socket_kernel_memory = 0;

  if (cpu->socket_kernel_scheduler > 0)
    close(cpu->socket_kernel_scheduler);
  cpu->socket_kernel_scheduler = 0;

  if (cpu->logger != NULL)
    log_destroy(cpu->logger);
  cpu->logger = NULL;

  if (cpu->config != NULL)
    config_destroy(cpu->config);
  cpu->config = NULL;

  if (cpu->handlers != NULL)
    dictionary_destroy(cpu->handlers);
  cpu->handlers = NULL;
}

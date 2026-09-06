#include "cpu/cleanup.h"

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/log.h"

void iterator_close_socket(void* value)
{
  close(*((int*)value));
  free(value);
}

void destroy_instruction(t_instruction* instruction)
{
  free(instruction->name);
  for (int i = 0; i < instruction->parameter_count; i++)
    free(instruction->parameters[i]);
  free(instruction);
}

void destroy_memory_stick(void* value)
{
  t_memory_stick_info* stick = (t_memory_stick_info*)value;
  if (stick->socket_ms > 0)
    close(stick->socket_ms);
  free(stick);
}

void close_module(t_cpu* cpu)
{
  if (cpu->memory_sticks != NULL)
    list_destroy_and_destroy_elements(cpu->memory_sticks, destroy_memory_stick);

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
    log_info(cpu->logger, "MODULE CLOSED");
    log_destroy(cpu->logger);
  }
  free(cpu);
}

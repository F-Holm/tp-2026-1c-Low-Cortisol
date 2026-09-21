#include "cpu/cleanup.h"

#include <stdlib.h>

#include "cpu/cpu.h"
#include "utils/collections/dictionary.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/sockets.h"

void iterator_close_socket(void* value)
{
  socket_destroy((t_socket*)value);
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
  socket_destroy(stick->socket_ms);
  free(stick);
}

void close_module(t_cpu* cpu)
{
  if (cpu->memory_sticks != NULL)
    list_destroy_and_destroy_elements(cpu->memory_sticks, destroy_memory_stick);

  socket_destroy(cpu->socket_kernel_memory);
  socket_destroy(cpu->socket_kernel_scheduler);

  if (cpu->config != NULL)
    config_destroy(cpu->config);

  if (cpu->handlers != NULL)
    dictionary_destroy(cpu->handlers);

  if (cpu->logger != NULL)
  {
    log_debug(cpu->logger, "CPU module closed");
    log_destroy(cpu->logger);
  }
  free(cpu);
}

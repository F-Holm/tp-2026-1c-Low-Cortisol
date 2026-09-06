#include <stdio.h>
#include <stdlib.h>

#include "cpu/connections.h"
#include "cpu/cpu.h"
#include "cpu/initializer.h"
#include "cpu/cleanup.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_cpu* cpu;
  cpu = malloc(sizeof(t_cpu));

  // verifica recibir correctamente los argumentos
  if (!check_arguments(argc, argv))
    return EXIT_FAILURE;

  char* config_path = argv[1];
  cpu->id = argv[2];

  if (!init_module(cpu, config_path))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (!connect_to_kernel_scheduler(cpu))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "String correctamente enviado al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger, "## fallo el envio del mensaje al kernel scheduler");
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (!connect_to_kernel_memory(cpu))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_memory))
  {
    log_info(cpu->logger, "String correctamente enviado al kernel memory");
  }
  else
  {
    log_error(cpu->logger, "## fallo el envio del mensaje al kernel memory");
    close_module(cpu);
    return EXIT_FAILURE;
  }
  log_info(cpu->logger, "Socket KM: %d", cpu->socket_kernel_memory);
  if (!receive_max_segment_size(cpu))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  // diccionario de intruciones (nombre - funcion)
  cpu->handlers = dictionary_create();
  register_handlers(cpu->handlers);

  run_instruction_loop(cpu);

  close_module(cpu);

  return 0;
}
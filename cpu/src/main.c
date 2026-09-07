#include <stdio.h>
#include <stdlib.h>

#include "cpu/cleanup.h"
#include "cpu/connections.h"
#include "cpu/cpu.h"
#include "cpu/initializer.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_cpu* cpu;
  cpu = malloc(sizeof(t_cpu));

  // Check that the arguments were received correctly.
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

  if (send_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_scheduler))
  {
    log_debug(cpu->logger, "ID sent to the Kernel Scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the message to the kernel scheduler");
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (!connect_to_kernel_memory(cpu))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  if (send_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_memory))
  {
    log_debug(cpu->logger, "ID sent to Kernel Memory");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the message to the kernel memory");
    close_module(cpu);
    return EXIT_FAILURE;
  }
  log_trace(cpu->logger, "Kernel Memory socket fd: %d",
            cpu->socket_kernel_memory);
  if (!receive_max_segment_size(cpu))
  {
    close_module(cpu);
    return EXIT_FAILURE;
  }

  // Instruction dictionary (name -> function).
  cpu->handlers = dictionary_create();
  register_handlers(cpu->handlers);

  run_instruction_loop(cpu);

  close_module(cpu);

  return 0;
}

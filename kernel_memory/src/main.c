#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/configurador.h"
#include "kernel_memory/inicializador.h"
#include "kernel_memory/liberador.h"
#include "kernel_memory/servidor.h"
#include "utils/kernel_memory_cpu.h"
#include "utils/msg.h"
#include "utils/server.h"

int main(int argc, char* argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];

  t_config* config = iniciar_config(archivo_config);
  t_log* logger = iniciar_logger(config);
  int socket_kernel_memory =
      iniciar_servidor(config_get_string_value(config, "PUERTO_KERNEL_MEMORY"));
  char* scripts_basepath = iniciar_basepath(config);
  int instruction_delay = iniciar_instruction_delay(config);
  int compaction_delay = iniciar_compaction_delay(config);
  int segment_max_size = iniciar_segment_max_size(config);
  int allocation_strategy = iniciar_allocation_strategy(config);

  t_datos_kernel_mem* datos_kernel = inicializar_datos_kernel_memory(
      socket_kernel_memory, scripts_basepath, instruction_delay,
      compaction_delay, segment_max_size, allocation_strategy, logger);

  pthread_mutex_lock(datos_kernel->mutex_logger);
  log_info(logger, "## Kernel Memory Iniciado ");
  pthread_mutex_unlock(datos_kernel->mutex_logger);

  while (true)
  {
    accept_cliente(datos_kernel);
  }

  liberar_datos_kernel_mem(datos_kernel);

  return 0;
}
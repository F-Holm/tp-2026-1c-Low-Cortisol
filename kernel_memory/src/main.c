#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/configurador.h"
#include "kernel_memory/inicializador.h"
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

  t_datos_kernel_mem* datos_kernel =
      inicializar_datos_kernel_memory(socket_kernel_memory, logger);

  while (true)
  {
    accept_cliente(datos_kernel);
  }

  pthread_mutex_destroy(&datos_kernel->mutex_lista_sockets);
  free(datos_kernel);

  return 0;
}
#include "kernel_memory/configurador.h"

#include <unistd.h>

#include "pthread.h"

t_log* iniciar_logger(t_config* config)
{
  return log_create(
      "kernel_memory.log", "kernel_memory", true,
      log_level_from_string(config_get_string_value(config, "LOG_LEVEL")));
}
t_config* iniciar_config(char* path)
{
  return config_create(path);
}
void terminar_comunicacion(int socket_cliente)
{
  close(socket_cliente);
}

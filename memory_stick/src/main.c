#include <commons/config.h>
#include <commons/log.h>

#include "memory_stick/kernel_memory.h"

int main(int argc, char* argv[])
{
  char* log_level;
  int memory_delay;
  char* ip;
  char* puerto;

  t_log* logger;
  t_config* config;

  config = config_create("memory_stick.config");

  log_level = config_get_string_value(config, "LOG_LEVEL");
  memory_delay config_get_int_value(config*, "MEMORY_DELAY");
  ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
  puerto = config_get_string_value(config, "KERNEL_MEMORY_PUERTO");

  logger = log_create("memory_stick.log", "memory_stick", true,
                      log_level_from_string(log_level));

  socket_kernel_memory = conectar_kernel_memory(ip, puerto);

  handshake(socket_kernel_memory);

  desconectar_kernel_memory(socket_kernel_memory);

  return 0;
}

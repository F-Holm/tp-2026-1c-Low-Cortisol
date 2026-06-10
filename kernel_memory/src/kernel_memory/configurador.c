#include "kernel_memory/configurador.h"

t_logger* iniciar_logger(t_config* config)
{
  return logger_create(
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

char* iniciar_basepath(t_config* config)
{
  return config_get_string_value(config, "SCRIPTS_BASEPATH");
}

int iniciar_instruction_delay(t_config* config)
{
  return config_get_int_value(config, "INSTRUCTION_DELAY");
}
int iniciar_compaction_delay(t_config* config)
{
  return config_get_int_value(config, "COMPACTION_DELAY");
}
int iniciar_segment_max_size(t_config* config)
{
  return config_get_int_value(config, "SEGMENT_MAX_SIZE");
}


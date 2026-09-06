#include "kernel_memory/configurator.h"

t_log* init_logger(t_config* config)
{
  return log_create(
      "kernel_memory.log", "kernel_memory", true,
      log_level_from_string(config_get_string_value(config, "LOG_LEVEL")),
      true);
}

t_config* init_config(char* path)
{
  return config_create(path);
}

void close_communication(int client_socket)
{
  close(client_socket);
}

char* get_scripts_basepath(t_config* config)
{
  return config_get_string_value(config, "SCRIPTS_BASEPATH");
}

int get_instruction_delay(t_config* config)
{
  return config_get_int_value(config, "INSTRUCTION_DELAY");
}

int get_compaction_delay(t_config* config)
{
  return config_get_int_value(config, "COMPACTION_DELAY");
}

int get_segment_max_size(t_config* config)
{
  return config_get_int_value(config, "SEGMENT_MAX_SIZE");
}

t_allocation_strategy get_allocation_strategy(t_config* config)
{
  return allocation_from_string(
      config_get_string_value(config, "ALLOCATION_STRATEGY"));
}

t_allocation_strategy allocation_from_string(char* strategy)
{
  if (strcmp(strategy, "BEST") == 0)
    return BEST;
  else if (strcmp(strategy, "WORST") == 0)
    return WORST;
  else
    return -1;
}

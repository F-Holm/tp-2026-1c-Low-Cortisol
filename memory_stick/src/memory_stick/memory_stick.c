#include "memory_stick/memory_stick.h"

bool open_confir_ms(t_config *config)
{
  config = config_create("memory_stick.config");
  return config != NULL;
}

void read_confir_ms(t_config *config, t_config_vars *config_vars)
{
  config_vars->log_level = config_get_string_value(config, "LOG_LEVEL");
  config_vars->memory_delay = config_get_int_value(config *, "MEMORY_DELAY");
  config_vars->ip_km = config_get_string_value(config, "KERNEL_MEMORY_IP");
  config_vars->puerto_km =
      config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
}

void close_confir_ms(t_config *config)
{
  config_destroy(config);
}

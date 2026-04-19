#include "memory_stick/memory_stick.h"

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"

bool conseguir_y_enviar_puerto(int socket_km, int socket_server_cpu,
                               t_log* logger)
{
  if (!enviar_puerto_server_ms_km(socket_km, get_puerto_cpu(socket_server_cpu)))
  {
    log_error(logger, "## Error en el envio del puerto del servidor para CPU");
    return false;
  }
  log_info(logger, "## Envio del puerto del servidor para CPU exitoso");
  return true;
}

bool iniciar_modulo(t_ms_recursos* ms_recursos, char* archivo_config,
                    char* tamanio)
{
  t_config_vars config_vars;

  // Config
  ms_recursos->config = iniciar_config(archivo_config, &config_vars);
  if (ms_recursos->config == NULL)
    return false;

  // Logger
  ms_recursos->logger =
      iniciar_logger(log_level_from_string(config_vars.log_level));
  if (ms_recursos->logger == NULL)
    return false;

  // Socket Kernel Memory
  ms_recursos->socket_km = iniciar_conexion_km(
      config_vars.ip_km, config_vars.puerto_km, tamanio, ms_recursos->logger);
  if (ms_recursos->socket_km <= 0)
    return false;

  // Enviar puerto del servidor a Memory Kernel
  ms_recursos->socket_server_cpu = create_server_cpu(ms_recursos->logger);
  if (ms_recursos->socket_server_cpu <= 0)
    return false;

  return conseguir_y_enviar_puerto(ms_recursos->socket_km,
                                   ms_recursos->socket_server_cpu,
                                   ms_recursos->logger);
}

t_config* iniciar_config(char* archivo_config, t_config_vars* config_vars)
{
  t_config* config = config_create(archivo_config);
  if (config != NULL)
    read_confir_ms(config, config_vars);
  return config;
}

t_log* iniciar_logger(t_log_level log_level)
{
  return log_create("memory_stick.log", "memory_stick", true, log_level);
}

void read_confir_ms(t_config* config, t_config_vars* config_vars)
{
  config_vars->log_level = config_get_string_value(config, "LOG_LEVEL");
  config_vars->memory_delay = config_get_int_value(config, "MEMORY_DELAY");
  config_vars->ip_km = config_get_string_value(config, "KERNEL_MEMORY_IP");
  config_vars->puerto_km =
      config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
}

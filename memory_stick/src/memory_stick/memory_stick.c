#include "memory_stick/memory_stick.h"

#include <stdlib.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "utils/msg.h"

bool send_cpu_server_port(int socket_km, int socket_server_cpu, t_log* logger)
{
  if (!send_cpu_server_port_to_km(socket_km, get_cpu_port(socket_server_cpu)))
  {
    log_error(logger, "## Error sending the CPU server port");
    return false;
  }
  log_debug(logger, "CPU server port sent successfully");
  return true;
}

bool init_module(t_ms* ms, char* config_path, char* size,
                 pthread_t* cpu_server_thread)
{
  t_config_vars config_vars;

  ms->config = init_config(config_path, &config_vars);
  if (ms->config == NULL)
    return false;

  ms->memory_delay = config_vars.memory_delay;

  ms->logger = init_logger(log_level_from_string(config_vars.log_level));
  if (ms->logger == NULL)
    return false;

  ms->socket_km = connect_to_kernel_memory(
      config_vars.km_ip, config_vars.km_port, size, ms->logger);
  if (ms->socket_km <= 0)
    return false;

  ms->socket_server_cpu = create_server_cpu(ms->logger);
  if (ms->socket_server_cpu <= 0)
    return false;

  if (!send_cpu_server_port(ms->socket_km, ms->socket_server_cpu, ms->logger))
    return false;

  // Reserve the amount of memory given in the config file.
  ms->memory = calloc(atoi(size), sizeof(char));
  ms->memory_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(ms->memory_mutex, NULL);

  // Thread that listens for new CPU connections.
  return start_cpu_server(cpu_server_thread, ms->socket_server_cpu, ms->logger,
                          ms);
}

t_config* init_config(char* config_path, t_config_vars* config_vars)
{
  t_config* config = config_create(config_path);
  if (config != NULL)
    read_config(config, config_vars);
  return config;
}

t_log* init_logger(t_log_level log_level)
{
  return log_create("memory_stick.log", "memory_stick", true, log_level, true);
}

void read_config(t_config* config, t_config_vars* config_vars)
{
  config_vars->log_level = config_get_string_value(config, "LOG_LEVEL");
  config_vars->memory_delay = config_get_int_value(config, "MEMORY_DELAY");
  config_vars->km_ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
  config_vars->km_port = config_get_string_value(config, "KERNEL_MEMORY_PORT");
}

void close_module_on_error(t_ms* ms)
{
  if (ms->socket_server_cpu > 0)
    close(ms->socket_server_cpu);
  if (ms->socket_km > 0)
    close(ms->socket_km);
  if (ms->logger != NULL)
    log_destroy(ms->logger);
  if (ms->config != NULL)
    config_destroy(ms->config);
  if (ms->memory != NULL)
    free(ms->memory);
  if (ms->memory_mutex != NULL)
  {
    pthread_mutex_destroy(ms->memory_mutex);
    free(ms->memory_mutex);
  }
}

void close_module(t_ms* ms, pthread_t* cpu_server_thread)
{
  shutdown(ms->socket_server_cpu, SHUT_RDWR);
  pthread_join(*cpu_server_thread, NULL);
  close(ms->socket_km);
  close(ms->socket_server_cpu);
  log_destroy(ms->logger);
  config_destroy(ms->config);
  free(ms->memory);
  pthread_mutex_destroy(ms->memory_mutex);
  free(ms->memory_mutex);
}

bool get_args(int argc, char** argv, char** config_path, char** size_str,
              int* size)
{
  if (argc != 3)
    return false;
  *config_path = argv[1];
  *size_str = argv[2];
  *size = atoi(*size_str);
  return *size > 0;
}

void read_memory(t_ms* ms, int start_position, int byte_count, int dest_socket)
{
  log_trace(ms->logger, "Reading %d bytes from offset %d", byte_count,
            start_position);
  char* bytes_to_return = calloc(byte_count + 1, 1);
  pthread_mutex_lock(ms->memory_mutex);
  memcpy(bytes_to_return, ms->memory + start_position, byte_count);
  pthread_mutex_unlock(ms->memory_mutex);
  usleep(ms->memory_delay * 1000);
  log_trace(ms->logger, "Read %d bytes", byte_count);
  send_buffer(OP_MEMORY_STICK_READ_DONE, bytes_to_return, byte_count,
              dest_socket);
  free(bytes_to_return);
}

void write_memory(t_ms* ms, int start_position, char* bytes_to_write,
                  int byte_count, int dest_socket)
{
  pthread_mutex_lock(ms->memory_mutex);
  memcpy(ms->memory + start_position, bytes_to_write, byte_count);
  pthread_mutex_unlock(ms->memory_mutex);
  log_trace(ms->logger, "Wrote %d bytes", byte_count);
  usleep(ms->memory_delay * 1000);
  send_string(OP_MEMORY_STICK_WRITE_DONE, "Write successful", dest_socket);
}

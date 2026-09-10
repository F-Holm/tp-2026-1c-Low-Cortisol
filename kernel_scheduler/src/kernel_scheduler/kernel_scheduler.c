#include "kernel_scheduler.h"

#include <pthread.h>
#include <string.h>

#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/server.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/string.h"

const char* const SCHEDULING_ALGORITHMS[] = {"FIFO", "RR", "CMN"};

static t_config* start_config(char* config_path, t_config_vars* config_vars);
static void close_config(t_config_vars* config_vars, t_config* config);
static t_log* start_logger(t_log_level log_level);

bool start_module(t_kernel_scheduler* recursos, char* config_path)
{
  // Config
  recursos->config = start_config(config_path, &(recursos->config_vars));
  if (recursos->config == NULL)
    return false;

  // Logger
  recursos->logger = start_logger(recursos->config_vars.log_level);
  if (recursos->logger == NULL)
    return false;

  // Socket Kernel Memory
  recursos->socket_kernel_memory = start_connection_kernel_memory(
      recursos->config_vars.kernel_memory_ip,
      recursos->config_vars.kernel_memory_port, recursos->logger);
  if (recursos->socket_kernel_memory <= 0)
    return false;

  // Create server socket
  recursos->socket_server =
      create_socket_server(recursos->config_vars.server_port, recursos->logger);

  return recursos->socket_server > 0;
}

void init_queues_mutex(t_kernel_scheduler* recursos)
{
  recursos->km_socket_mutex =
      init_socket_kernel_memory(recursos->socket_kernel_memory);
  recursos->connection_check_thread_data =
      start_thread_check_connection_kernel_memory(
          recursos->socket_server, recursos->logger, recursos->km_socket_mutex);
  recursos->mutex_list = init_list_mutex();
  recursos->queues = init_queues(
      recursos->config_vars.scheduling_algorithm,
      recursos->config_vars.cmn_algorithms, recursos->config_vars.rr_quantum,
      recursos->config_vars.preemption, recursos->socket_server,
      recursos->logger, recursos->km_socket_mutex,
      recursos->config_vars.suspension_timeout);
}

void close_module_error(t_kernel_scheduler* recursos)
{
  if (recursos->socket_server > 0)
  {
    close(recursos->socket_server);
  }
  if (recursos->socket_kernel_memory > 0)
  {
    close(recursos->socket_kernel_memory);
  }
  if (recursos->logger != NULL)
  {
    log_destroy(recursos->logger);
  }
  if (recursos->config != NULL)
  {
    close_config(&(recursos->config_vars), recursos->config);
  }
}

void close_module(t_kernel_scheduler* recursos)
{
  destroy_list_mutex(recursos->mutex_list);
  clear_queues(recursos->queues);
  destroy_queues(recursos->queues);
  destroy_thread_check_connection_kernel_memory(
      recursos->connection_check_thread_data);
  destroy_kernel_memory(recursos->km_socket_mutex);
  close(recursos->socket_kernel_memory);
  close(recursos->socket_server);
  log_destroy(recursos->logger);
  close_config(&(recursos->config_vars), recursos->config);
}

static t_config* start_config(char* config_path, t_config_vars* config_vars)
{
  int i;
  t_config* config = config_create(config_path);
  if (config != NULL)
  {
    config_vars->log_level =
        log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));

    char* algorithm_scheduling_str =
        config_get_string_value(config, "SCHEDULING_ALGORITHM");
    for (i = 0; i < 3; i++)
    {
      if (strcmp(algorithm_scheduling_str, SCHEDULING_ALGORITHMS[i]) == 0)
      {
        config_vars->scheduling_algorithm = i;
        break;
      }
    }

    char** array_str = config_get_array_value(config, "QUEUE_ALGORITHMS");
    config_vars->cmn_algorithms = list_create();
    i = 0;
    while (array_str[i] != NULL)
    {
      int* algorithm = malloc(sizeof(int));
      if (strcmp(array_str[i], SCHEDULING_ALGORITHMS[AP_FIFO]) == 0)
        *algorithm = AP_FIFO;
      else if (strcmp(array_str[i], SCHEDULING_ALGORITHMS[AP_RR]) == 0)
        *algorithm = AP_RR;
      list_add(config_vars->cmn_algorithms, algorithm);
      i++;
    }
    string_array_destroy(array_str);

    config_vars->rr_quantum = config_get_int_value(config, "RR_QUANTUM");

    config_vars->preemption =
        strcmp(config_get_string_value(config, "QUEUE_PREEMPTION"), "TRUE") ==
        0;

    config_vars->suspension_timeout =
        config_get_int_value(config, "SUSPENSION_TIMEOUT");

    config_vars->server_port =
        config_get_string_value(config, "KERNEL_SCHEDULER_PORT");

    config_vars->kernel_memory_ip =
        config_get_string_value(config, "KERNEL_MEMORY_IP");

    config_vars->kernel_memory_port =
        config_get_string_value(config, "KERNEL_MEMORY_PORT");
  }
  return config;
}

static void close_config(t_config_vars* config_vars, t_config* config)
{
  list_destroy_and_destroy_elements(config_vars->cmn_algorithms, free);
  config_destroy(config);
}

static t_log* start_logger(t_log_level log_level)
{
  return log_create("kernel_scheduler.log", "kernel_scheduler", true, log_level,
                    true);
}

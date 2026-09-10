#include "kernel_scheduler/app/kernel_scheduler.h"

#include <pthread.h>
#include <string.h>

#include "kernel_scheduler/connections/kernel_memory.h"
#include "kernel_scheduler/connections/server.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/string.h"

const char* const SCHEDULING_ALGORITHMS[] = {"FIFO", "RR", "CMN"};

static t_config* start_config(char* config_path, t_config_vars* config_vars);
static void close_config(t_config_vars* config_vars, t_config* config);
static t_log* start_logger(t_log_level log_level);

bool start_module(t_kernel_scheduler* resources, char* config_path)
{
  // Config
  resources->config = start_config(config_path, &(resources->config_vars));
  if (resources->config == NULL)
    return false;

  // Logger
  resources->logger = start_logger(resources->config_vars.log_level);
  if (resources->logger == NULL)
    return false;

  // Socket Kernel Memory
  resources->socket_kernel_memory = start_connection_kernel_memory(
      resources->config_vars.kernel_memory_ip,
      resources->config_vars.kernel_memory_port, resources->logger);
  if (resources->socket_kernel_memory <= 0)
    return false;

  // Create server socket
  resources->socket_server =
      create_socket_server(resources->config_vars.server_port, resources->logger);

  return resources->socket_server > 0;
}

void init_scheduler_resources(t_kernel_scheduler* resources)
{
  resources->km_socket_mutex =
      init_socket_kernel_memory(resources->socket_kernel_memory);
  resources->connection_check_thread_data =
      start_thread_check_connection_kernel_memory(
          resources->socket_server, resources->logger, resources->km_socket_mutex);
  resources->mutex_list = init_list_mutex();
  resources->queues = init_queues(
      resources->config_vars.scheduling_algorithm,
      resources->config_vars.cmn_algorithms, resources->config_vars.rr_quantum,
      resources->config_vars.preemption, resources->socket_server,
      resources->logger, resources->km_socket_mutex,
      resources->config_vars.suspension_timeout);
}

void close_module_error(t_kernel_scheduler* resources)
{
  if (resources->socket_server > 0)
  {
    close(resources->socket_server);
  }
  if (resources->socket_kernel_memory > 0)
  {
    close(resources->socket_kernel_memory);
  }
  if (resources->logger != NULL)
  {
    log_destroy(resources->logger);
  }
  if (resources->config != NULL)
  {
    close_config(&(resources->config_vars), resources->config);
  }
}

void close_module(t_kernel_scheduler* resources)
{
  destroy_list_mutex(resources->mutex_list);
  clear_queues(resources->queues);
  destroy_queues(resources->queues);
  destroy_thread_check_connection_kernel_memory(
      resources->connection_check_thread_data);
  destroy_kernel_memory(resources->km_socket_mutex);
  close(resources->socket_kernel_memory);
  close(resources->socket_server);
  log_destroy(resources->logger);
  close_config(&(resources->config_vars), resources->config);
}

static t_config* start_config(char* config_path, t_config_vars* config_vars)
{
  int i;
  t_config* config = config_create(config_path);
  if (config != NULL)
  {
    config_vars->log_level =
        log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));

    char* scheduling_algorithm_str =
        config_get_string_value(config, "SCHEDULING_ALGORITHM");
    for (i = 0; i < 3; i++)
    {
      if (strcmp(scheduling_algorithm_str, SCHEDULING_ALGORITHMS[i]) == 0)
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

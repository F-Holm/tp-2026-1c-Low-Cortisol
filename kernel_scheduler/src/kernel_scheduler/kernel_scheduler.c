#include "kernel_scheduler.h"

#include <commons/config.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <string.h>

#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/server.h"
#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

const char* const ALGORITMOS_PLANIFICACION[] = {"FIFO", "RR", "CMN"};

static t_config* iniciar_config(char* archivo_config,
                                t_config_vars* config_vars)
{
  int i;
  t_config* config = config_create(archivo_config);
  if (config != NULL)
  {
    config_vars->log_level =
        log_level_from_string(config_get_string_value(config, "LOG_LEVEL"));

    char* algoritmo_planificacion_str =
        config_get_string_value(config, "PLANIFICATION_ALGORITHM");
    for (i = 0; i < 3; i++)
    {
      if (strcmp(algoritmo_planificacion_str, ALGORITMOS_PLANIFICACION[i]) == 0)
      {
        config_vars->algoritmo_planificacion = i;
        break;
      }
    }

    char** array_str = config_get_array_value(config, "QUEUES_ALGORITHMS");
    config_vars->algoritmos_cmn = list_create();
    i = 0;
    while (array_str[i] != NULL)
    {
      int* algoritmo = malloc(sizeof(int));
      if (strcmp(array_str[i], ALGORITMOS_PLANIFICACION[AP_FIFO]))
        *algoritmo = AP_FIFO;
      else if (strcmp(array_str[i], ALGORITMOS_PLANIFICACION[AP_RR]))
        *algoritmo = AP_RR;
      list_add(config_vars->algoritmos_cmn, algoritmo);
      i++;
    }
    string_array_destroy(array_str);

    config_vars->rr_quantum = config_get_int_value(config, "RR_QUANTUM");

    config_vars->desalojo =
        strcmp(config_get_string_value(config, "QUEUE_PREEMPTION"), "TRUE") ==
        0;

    config_vars->suspension_timeout =
        config_get_int_value(config, "SUSPENSION_TIMEOUT");

    config_vars->puerto_servidor =
        config_get_string_value(config, "KERNEL_SCHEDULER_PUERTO");

    config_vars->ip_kernel_memory =
        config_get_string_value(config, "KERNEL_MEMORY_IP");

    config_vars->puerto_kernel_memory =
        config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
  }
  return config;
}

static void cerrar_config(t_config_vars* config_vars, t_config* config)
{
  list_clean_and_destroy_elements(config_vars->algoritmos_cmn, free);
  config_destroy(config);
}

static t_logger* iniciar_logger(t_log_level log_level)
{
  return logger_create("kernel_scheduler.log", "kernel_scheduler", true,
                       log_level);
}

bool iniciar_modulo(t_kernel_scheduler_recursos* recursos, char* archivo_config)
{
  // Config
  recursos->config = iniciar_config(archivo_config, &(recursos->config_vars));
  if (recursos->config == NULL)
    return false;

  // Logger
  recursos->logger = iniciar_logger(recursos->config_vars.log_level);
  if (recursos->logger == NULL)
    return false;

  // Socket Kernel Memory
  recursos->socket_kernel_memory = iniciar_conexion_kernel_memory(
      recursos->config_vars.ip_kernel_memory,
      recursos->config_vars.puerto_kernel_memory, recursos->logger);
  if (recursos->socket_kernel_memory <= 0)
    return false;

  // Crear socket servidor
  recursos->socket_server = crear_socket_servidor(
      recursos->config_vars.puerto_servidor, recursos->logger);

  return recursos->socket_server > 0;
}

void inicializar_colas_mutex(t_kernel_scheduler_recursos* recursos)
{
  inicializar_mutex_pid_pcb();
  inicializar_mutex_shutdown();
  recursos->socket_km_mutex =
      inicializar_socket_kernel_memory(recursos->socket_kernel_memory);
  recursos->lista_mutex = inicializar_lista_mutex();
  recursos->colas = inicializar_colas(
      recursos->config_vars.algoritmo_planificacion,
      recursos->config_vars.algoritmos_cmn, recursos->config_vars.rr_quantum,
      recursos->config_vars.desalojo, recursos->socket_server,
      recursos->logger);
}

void cerrar_modulo_error(t_kernel_scheduler_recursos* recursos)
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
    logger_destroy(recursos->logger);
  }
  if (recursos->config != NULL)
  {
    cerrar_config(&(recursos->config_vars), recursos->config);
  }
}

void cerrar_modulo(t_kernel_scheduler_recursos* recursos)
{
  destruir_lista_mutex(recursos->lista_mutex);
  vaciar_colas(recursos->colas, recursos->logger, recursos->socket_km_mutex,
               recursos->socket_server);
  destruir_colas(recursos->colas);
  destruir_kernel_memory(recursos->socket_km_mutex);
  close(recursos->socket_kernel_memory);
  close(recursos->socket_server);
  logger_destroy(recursos->logger);
  cerrar_config(&(recursos->config_vars), recursos->config);
  destruir_mutex_pid_pcb();
  destruir_mutex_shutdown();
}

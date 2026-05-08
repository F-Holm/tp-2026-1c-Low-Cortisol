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

t_config* iniciar_config(char* archivo_config, t_config_vars* config_vars)
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

void cerrar_config(t_config_vars* config_vars, t_config* config)
{
  list_clean_and_destroy_elements(config_vars->algoritmos_cmn, free);
  config_destroy(config);
}

t_log* iniciar_logger(t_log_level log_level)
{
  return log_create("kernel_scheduler.log", "kernel_scheduler", true,
                    log_level);
}

t_datos_hilo_escucha* inicializar_datos_hilo_escucha_recursos(
    t_kernel_scheduler_recursos* recursos)
{
  return inicializar_datos_hilo_escucha(recursos->socket_server,
                                        recursos->logger);
}

bool crear_servidor(pthread_t* hilo_servidor, t_datos_hilo_escucha* datos)
{
  if (pthread_create(hilo_servidor, NULL, hilo_escucha, datos) != 0)
  {
    log_error(datos->logger, "## Error al crear el hilo del servidor");
    return false;
  }
  return true;
}

bool iniciar_modulo(t_kernel_scheduler_recursos* recursos, char* archivo_config,
                    t_cola_mutex_io* cola_mutex)
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

  // Crear servidor
  recursos->socket_server = crear_socket_servidor(
      recursos->config_vars.puerto_servidor, recursos->logger);
  if (recursos->socket_server <= 0)
    return false;
  return crear_servidor(&(recursos->hilo_servidor),
                        inicializar_datos_hilo_escucha_recursos(recursos));
}

void cerrar_modulo_error(t_kernel_scheduler_recursos* recursos)
{
  if (recursos->socket_server > 0)
    close(recursos->socket_server);
  if (recursos->socket_kernel_memory > 0)
    close(recursos->socket_kernel_memory);
  if (recursos->logger != NULL)
    log_destroy(recursos->logger);
  if (recursos->config != NULL)
    cerrar_config(&(recursos->config_vars), recursos->config);
}

void cerrar_modulo(t_kernel_scheduler_recursos* recursos)
{
  shutdown(recursos->socket_server, SHUT_RDWR);
  pthread_join(recursos->hilo_servidor, NULL);
  close(recursos->socket_kernel_memory);
  close(recursos->socket_server);
  log_destroy(recursos->logger);
  cerrar_config(&(recursos->config_vars), recursos->config);
}

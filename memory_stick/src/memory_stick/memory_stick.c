#include "memory_stick/memory_stick.h"

#include <stdlib.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "utils/msg.h"

bool conseguir_y_enviar_puerto(int socket_km, int socket_server_cpu,
                               t_log* logger)
{
  if (!enviar_puerto_server_ms_km(socket_km, get_puerto_cpu(socket_server_cpu)))
  {
    log_error(logger, "## Error en el envio del puerto del servidor para CPU");
    return false;
  }
  log_info(logger, "Envio del puerto del servidor para CPU exitoso");
  return true;
}

bool iniciar_modulo(t_ms_recursos* ms_recursos, char* archivo_config,
                    char* tamanio, pthread_t* hilo_server_cpu)
{
  t_config_vars config_vars;

  // Config
  ms_recursos->config = iniciar_config(archivo_config, &config_vars);
  if (ms_recursos->config == NULL)
    return false;

  // Config vars
  ms_recursos->memory_delay = config_vars.memory_delay;

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

  // Enviar puerto del servidor a Kernel Memory
  if (!conseguir_y_enviar_puerto(ms_recursos->socket_km,
                                 ms_recursos->socket_server_cpu,
                                 ms_recursos->logger))
    return false;

  // Reservar la memoria indicada en el archivo config
  ms_recursos->memoria = calloc(atoi(tamanio), sizeof(char));
  // Mutex
  ms_recursos->mutex_memoria = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(ms_recursos->mutex_memoria, NULL);

  // Hilo para escuchar nuevas conexiones de CPUs
  return crear_servidor_cpu(hilo_server_cpu, ms_recursos->socket_server_cpu,
                            ms_recursos->logger, ms_recursos);
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
  return log_create("memory_stick.log", "memory_stick", true, log_level, true);
}

void read_confir_ms(t_config* config, t_config_vars* config_vars)
{
  config_vars->log_level = config_get_string_value(config, "LOG_LEVEL");
  config_vars->memory_delay = config_get_int_value(config, "MEMORY_DELAY");
  config_vars->ip_km = config_get_string_value(config, "KERNEL_MEMORY_IP");
  config_vars->puerto_km =
      config_get_string_value(config, "KERNEL_MEMORY_PUERTO");
}

void cerrar_modulo_error(t_ms_recursos* ms_recursos)
{
  if (ms_recursos->socket_server_cpu > 0)
    close(ms_recursos->socket_server_cpu);
  if (ms_recursos->socket_km > 0)
    close(ms_recursos->socket_km);
  if (ms_recursos->logger != NULL)
    log_destroy(ms_recursos->logger);
  if (ms_recursos->config != NULL)
    config_destroy(ms_recursos->config);
  if (ms_recursos->memoria != NULL)
    free(ms_recursos->memoria);
  if (ms_recursos->mutex_memoria != NULL)
  {
    pthread_mutex_destroy(ms_recursos->mutex_memoria);
    free(ms_recursos->mutex_memoria);
  }
}

void cerrar_modulo(t_ms_recursos* ms_recursos, pthread_t* thread_server_cpu)
{
  shutdown(ms_recursos->socket_server_cpu, SHUT_RDWR);
  pthread_join(*thread_server_cpu, NULL);
  close(ms_recursos->socket_km);
  close(ms_recursos->socket_server_cpu);
  log_destroy(ms_recursos->logger);
  config_destroy(ms_recursos->config);
  free(ms_recursos->memoria);
  pthread_mutex_destroy(ms_recursos->mutex_memoria);
  free(ms_recursos->mutex_memoria);
}

bool get_args(int argc, char** argv, char** archivo_config, char** tamanio_str,
              int* tamanio)
{
  if (argc != 3)
    return false;
  *archivo_config = argv[1];
  *tamanio_str = argv[2];
  *tamanio = atoi(*tamanio_str);
  return tamanio > 0;
}

void leer_memoria(t_ms_recursos* ms_recursos, int posicion_inicial,
                  int cantidad_de_bytes, int socket_destino)
{
  log_info(ms_recursos->logger, "Memory stick necesita leer %d bytes, desde %d",
           cantidad_de_bytes, posicion_inicial);
  char* bytes_a_devolver = calloc(cantidad_de_bytes + 1, 1);
  pthread_mutex_lock(ms_recursos->mutex_memoria);
  memcpy(bytes_a_devolver, ms_recursos->memoria + posicion_inicial,
         cantidad_de_bytes);
  pthread_mutex_unlock(ms_recursos->mutex_memoria);
  usleep(ms_recursos->memory_delay * 1000);
  log_info(ms_recursos->logger, "Memory stick leyo los bytes, %s",
           bytes_a_devolver);
  enviar_buffer(OP_MEMORY_STICK_LEIDO, bytes_a_devolver, cantidad_de_bytes,
                socket_destino);
  free(bytes_a_devolver);
}

void escribir_memoria(t_ms_recursos* ms_recursos, int posicion_inicial,
                      char* bytes_a_escribir, int cantidad_de_bytes,
                      int socket_destino)
{
  pthread_mutex_lock(ms_recursos->mutex_memoria);
  memcpy(ms_recursos->memoria + posicion_inicial, bytes_a_escribir,
         cantidad_de_bytes);
  pthread_mutex_unlock(ms_recursos->mutex_memoria);
  log_info(ms_recursos->logger, "Se escribieron %d bytes", cantidad_de_bytes);
  usleep(ms_recursos->memory_delay * 1000);
  enviar_string(OP_MEMORY_STICK_ESCRITO, "Escritura Exitosa", socket_destino);
}

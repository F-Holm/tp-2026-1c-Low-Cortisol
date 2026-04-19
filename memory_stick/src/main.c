#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "memory_stick/memory_stick.h"
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  int socket_km;
  int socket_server_cpu;
} t_ms_recursos;

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

bool handshake(int socket_km, t_log* logger)
{
  if (!enviar_handshake(MID_MEMORY_STICK, socket_km))
  {
    log_error(logger, "## Error en el envio del Handshake con Kernel Memory");
    return false;
  }
  if (recibir_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    log_error(logger,
              "## Error en la recepción del Handshake con Kernel Memory");
    return false;
  }
  log_info(logger, "## Handshake exitoso con Kernel Memory");
  return true;
}

bool enviar_tamanio(int socket_km, char* tamanio, t_log* logger)
{
  if (!enviar_string(OP_TAMANIO_MEMORIA, tamanio, socket_km))
  {
    log_error(logger, "## Error en el envio de tamaño");
    return false;
  }
  log_info(logger, "## Envio de tamaño exitoso");
  return true;
}

int iniciar_conexion_km(char* ip, char* puerto, char* tamanio, t_log* logger)
{
  int socket_km = conectar_km(ip, puerto, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake(socket_km, logger))
    return -1;

  if (!enviar_tamanio(socket_km, tamanio, logger))
    return -1;

  return socket_km;
}

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

int main(int argc, char* argv[])
{
  t_ms_recursos ms_recursos = {0};
  pthread_t thread_server_cpu;

  // args
  if (argc != 3)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  char* tamanio_str = argv[2];
  int tamanio = atoi(tamanio_str);
  if (tamanio <= 0)
    return EXIT_FAILURE;

  if (!iniciar_modulo(&ms_recursos, archivo_config, tamanio_str))
  {
    if (ms_recursos.socket_server_cpu > 0)
      close(ms_recursos.socket_server_cpu);
    if (ms_recursos.socket_km > 0)
      close(ms_recursos.socket_km);
    if (ms_recursos.logger != NULL)
      log_destroy(ms_recursos.logger);
    if (ms_recursos.config != NULL)
      config_destroy(ms_recursos.config);
    return EXIT_FAILURE;
  }

  // Hilo para escuchar nuevas conexiones de CPUs
  t_datos_hilo_escucha datos_hilo_escucha;
  datos_hilo_escucha.socket_fd = ms_recursos.socket_server_cpu;
  datos_hilo_escucha.logger = ms_recursos.logger;
  pthread_create(&thread_server_cpu, NULL, hilo_escucha_cpu,
                 &datos_hilo_escucha);

  // Esperando Instrucciones del Kernel Memory
  while (true)
  {
    int op_code = recibir_operacion(ms_recursos.socket_km);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(ms_recursos.socket_km);
    free(buffer);
  }

  // Liberar y Cerrar
  shutdown(ms_recursos.socket_server_cpu, SHUT_RDWR);
  pthread_join(thread_server_cpu, NULL);
  liberar_conexion(ms_recursos.socket_km);
  liberar_conexion(ms_recursos.socket_server_cpu);
  log_destroy(ms_recursos.logger);
  config_destroy(ms_recursos.config);
  return EXIT_SUCCESS;
}

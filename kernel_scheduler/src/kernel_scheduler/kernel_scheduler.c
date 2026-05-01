#include "kernel_scheduler.h"

#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <string.h>

#include "cpu.h"
#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

t_log* logger;
t_config* config;

t_log* iniciar_logger(t_config* config)
{
  t_log* nuevo_logger;
  char* log_levelstr = config_get_string_value(config, "LOG_LEVEL");
  t_log_level log_level = log_level_from_string(log_levelstr);
  nuevo_logger =
      log_create("kernel_scheduler.log", "kernelScheduler", true, log_level);
  return nuevo_logger;
}

t_config* iniciar_config(char* path)
{
  t_config* nuevo_config;
  nuevo_config = config_create(path);
  return nuevo_config;
}

void iniciar_modulo(t_k_scheduler_recursos* k_scheduler_recursos,
                    char* archivo_config)
{
  k_scheduler_recursos->config = iniciar_config(archivo_config);
  k_scheduler_recursos->logger = iniciar_logger(k_scheduler_recursos->config);
  k_scheduler_recursos->ip =
      config_get_string_value(k_scheduler_recursos->config, "KERNEL_MEMORY_IP");
  k_scheduler_recursos->puerto = config_get_string_value(
      k_scheduler_recursos->config, "KERNEL_MEMORY_PUERTO");
  k_scheduler_recursos->puerto_servidor = config_get_string_value(
      k_scheduler_recursos->config, "KERNEL_SCHEDULER_PUERTO");
}

bool conectar_kernel_memory(t_k_scheduler_recursos* k_scheduler_recursos)
{
  k_scheduler_recursos->socket_km =
      crear_conexion(k_scheduler_recursos->ip, k_scheduler_recursos->puerto);
  if (k_scheduler_recursos->socket_km <= 0)
  {
    log_error(k_scheduler_recursos->logger,
              "## Fallo la conexion con kernel memory en %s:%s",
              k_scheduler_recursos->ip, k_scheduler_recursos->puerto);
    cerrar_modulo(k_scheduler_recursos);
    return false;
  }
  else
  {
    log_info(k_scheduler_recursos->logger,
             "##Conexion establecida con kernel memory en %s:%s",
             k_scheduler_recursos->ip, k_scheduler_recursos->puerto);
    return true;
  }
}

bool handshake_kernel_memory(t_k_scheduler_recursos* k_scheduler_recursos)
{
  enviar_handshake(MID_KERNEL_SCHEDULER, k_scheduler_recursos->socket_km);
  int id_modulo = recibir_handshake(k_scheduler_recursos->socket_km);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(k_scheduler_recursos->logger,
              "## Error en el Handshake con Kernel Memory");
    close(k_scheduler_recursos->socket_km);
    log_destroy(k_scheduler_recursos->logger);
    config_destroy(k_scheduler_recursos->config);
    return false;
  }
  log_info(k_scheduler_recursos->logger,
           "## Handshake exitoso con Kernel Memory");
  return true;
}

void iniciar_servidor_cpu_io(t_k_scheduler_recursos* k_scheduler_recursos,
                             t_datos_hilo_escucha* datos_hilo_escucha)
{
  datos_hilo_escucha->socket_fd = k_scheduler_recursos->server;
  datos_hilo_escucha->logger = k_scheduler_recursos->logger;
  pthread_create(&k_scheduler_recursos->thread_server, NULL,
                 hilo_escucha_server, datos_hilo_escucha);
  log_info(k_scheduler_recursos->logger,
           "## Servidor listo para recibir CPUs e IOs");
}

void cerrar_modulo(t_k_scheduler_recursos* k_scheduler_recursos)
{
  shutdown(k_scheduler_recursos->server, SHUT_RDWR);
  pthread_join(k_scheduler_recursos->thread_server, NULL);
  log_info(k_scheduler_recursos->logger, "## Servidor cerrado.");
  liberar_conexion(k_scheduler_recursos->server);
  liberar_conexion(k_scheduler_recursos->socket_km);
  log_destroy(k_scheduler_recursos->logger);
  config_destroy(k_scheduler_recursos->config);
}

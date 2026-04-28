#include "kernel_scheduler.h"

#include <pthread.h>
#include <commons/config.h>
#include <commons/log.h>
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

void iniciar_modulo(t_kScheduler_recursos* kScheduler_recursos,
                    char* archivo_config)
{
  kScheduler_recursos->config = iniciar_config(archivo_config);
  kScheduler_recursos->logger = iniciar_logger(kScheduler_recursos->config);
  kScheduler_recursos->ip =
      config_get_string_value(kScheduler_recursos->config, "KERNEL_MEMORY_IP");
  kScheduler_recursos->puerto = config_get_string_value(
      kScheduler_recursos->config, "KERNEL_MEMORY_PUERTO");
  kScheduler_recursos->puerto_servidor = config_get_string_value(
      kScheduler_recursos->config, "KERNEL_SCHEDULER_PUERTO");
}

bool conectar_kernel_memory(t_kScheduler_recursos* kScheduler_recursos)
{
  kScheduler_recursos->socket_km =
      crear_conexion(kScheduler_recursos->ip, kScheduler_recursos->puerto);
  if (kScheduler_recursos->socket_km <= 0)
  {
    log_error(kScheduler_recursos->logger,
              "## Fallo la conexion con kernel memory en %s:%s",
              kScheduler_recursos->ip, kScheduler_recursos->puerto);
    cerrar_modulo(kScheduler_recursos);
    return false;
  }
  else
  {
    log_info(kScheduler_recursos->logger,
             "##Conexion establecida con kernel memory en %s:%s",
             kScheduler_recursos->ip, kScheduler_recursos->puerto);
    return true;
  }
}

bool handshake_kernel_memory(t_kScheduler_recursos* kScheduler_recursos)
{
  enviar_handshake(MID_KERNEL_SCHEDULER, kScheduler_recursos->socket_km);
  int id_modulo = recibir_handshake(kScheduler_recursos->socket_km);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(kScheduler_recursos->logger,
              "## Error en el Handshake con Kernel Memory");
    close(kScheduler_recursos->socket_km);
    log_destroy(kScheduler_recursos->logger);
    config_destroy(kScheduler_recursos->config);
    return false;
  }
  log_info(kScheduler_recursos->logger,
           "## Handshake exitoso con Kernel Memory");
  return true;
}

void iniciar_servidor_cpu_io(t_kScheduler_recursos* kScheduler_recursos,
                             t_datos_hilo_escucha* datos_hilo_escucha)
{
  datos_hilo_escucha->socket_fd = kScheduler_recursos->server;
  datos_hilo_escucha->logger = kScheduler_recursos->logger;
  pthread_create(&kScheduler_recursos->thread_server, NULL, hilo_escucha_server,
                 &datos_hilo_escucha);
  log_info(kScheduler_recursos->logger,
           "## Servidor listo para recibir CPUs e IOs");
}

void cerrar_modulo(t_kScheduler_recursos* kScheduler_recursos)
{
  shutdown(kScheduler_recursos->server, SHUT_RDWR);
  pthread_join(kScheduler_recursos->thread_server, NULL);
  log_info(kScheduler_recursos->logger, "## Servidor de cpu cerrado.");
  liberar_conexion(kScheduler_recursos->server);
  liberar_conexion(kScheduler_recursos->socket_km);
  log_destroy(kScheduler_recursos->logger);
  config_destroy(kScheduler_recursos->config);
}
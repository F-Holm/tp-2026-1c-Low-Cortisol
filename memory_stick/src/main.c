#include <arpa/inet.h>
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

int fd_kernel;
int fd_escucha_server;
volatile bool seguir_operando = true;

int main(int argc, char* argv[])
{
  t_config_vars* config_vars;
  t_config* config;
  t_log* logger;
  int socket_km;
  int socket_sv_ms;
  uint16_t puerto_server_ms;
  pthread_t thread_sv_ms;

  // Config
  if (!open_confir_ms(config))
    return EXIT_FAILURE;
  read_confir_ms(config, config_vars);

  // Logger
  logger = log_create("memory_stick.log", "memory_stick", true,
                      log_level_from_string(config_vars->log_level));
  if (logger == NULL)
  {
    close_confir_ms(config);
    return EXIT_FAILURE;
  }

  // Socket Kernel Memory
  socket_km = conectar_km(config_vars->ip_km, config_vars->puerto_km);
  if (socket_km < 0)
  {
    close_confir_ms(config);
    log_destroy(logger);
    return EXIT_FAILURE;
  }

  // Handshake con Kernel Memory
  if (!handshake_km(socket_km))
  {
    close(socket_km);
    close_confir_ms(config);
    log_destroy(logger);
    return EXIT_FAILURE;
  }

  // Enviar puerto del servidor a Memory Kernel
  socket_sv_ms = create_server_cpu();
  puerto_server_ms = get_puerto_cpu(socket_sv_ms);
  enviar_puerto_server_ms_km(socket_km, puerto_server_ms);

  // Hilo para escuchar nuevas conexiones de CPUs
  pthread_create(&thread_sv_ms, NULL, hilo_escucha_cpus, &socket_sv_ms);

  // Esperando Instrucciones del Kernel Memory
  while (seguir_operando)
  {
    int len;
    if (recv(fd_kernel, &len, sizeof(int), MSG_WAITALL) <= 0)
    {
      log_info(logger, "Kernel desconectado. Iniciando cierre...");
      seguir_operando = false;
    }
  }

  // Liberar y Cerrar
  shutdown(thread_sv_ms, SHUT_RDWR);
  liberar_conexion(socket_km);
  liberar_conexion(socket_sv_ms);
  pthread_join(thread_sv_ms, NULL);
  log_destroy(logger);
  close_confir_ms(config);
  return EXIT_SUCCESS;
}

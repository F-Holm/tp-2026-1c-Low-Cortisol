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
#include "utils/msg.h"

typedef struct
{
  int socket_fd;
  t_log* logger;
} t_datos_hilo_escucha;

volatile bool seguir_operando = true;

void* hilo_escucha_cpu(void* datos_hilo_escucha_void);

int main(int argc, char* argv[])
{
  t_config_vars* config_vars;
  t_config* config;
  t_log* logger;
  int socket_km;
  int socket_server_cpu;
  uint16_t puerto_server_cpu;
  pthread_t thread_server_cpu;
  t_log_level log_level;

  // Config
  config = config_create("memory_stick.config");
  if (config == NULL)
    return EXIT_FAILURE;
  read_confir_ms(config, config_vars);

  // Logger
  log_level = log_level_from_string(config_vars->log_level);
  logger = log_create("memory_stick.log", "memory_stick", true, log_level);
  if (logger == NULL)
  {
    config_destroy(config);
    return EXIT_FAILURE;
  }

  // Socket Kernel Memory
  socket_km = conectar_km(config_vars->ip_km, config_vars->puerto_km);
  if (socket_km < 0)
  {
    log_error(logger, "## Error de conexión al Kernel_Memory");
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Conectado a Kernel Memory");

  // Handshake con Kernel Memory
  enviar_handshake(MID_MEMORY_STICK, socket_km);
  int id_modulo = recibir_handshake(socket_km);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(logger, "## Error en el Handshake con Kernel_Memory");
    close(socket_km);
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Handshake exitoso con Kernel Memory");

  // Enviar puerto del servidor a Memory Kernel
  socket_server_cpu = create_server_cpu();
  puerto_server_cpu = get_puerto_cpu(socket_server_cpu);
  enviar_puerto_server_ms_km(socket_km, puerto_server_cpu);

  // Hilo para escuchar nuevas conexiones de CPUs
  t_datos_hilo_escucha datos_hilo_escucha;
  datos_hilo_escucha.socket_fd = socket_server_cpu;
  datos_hilo_escucha.logger = logger;
  pthread_create(&thread_server_cpu, NULL, hilo_escucha_cpu,
                 &datos_hilo_escucha);

  // Esperando Instrucciones del Kernel Memory
  while (seguir_operando)
  {
    int op_code = recibir_operacion(socket_km);
    char* buffer;
    switch (op_code)
    {
      case OP_CODE_ERROR:
        seguir_operando = false;
        break;
      default:
        buffer = recibir_string(socket_km);
        // log_info(logger, "%s", buffer);
        free(buffer);
        break;
    }
  }

  // Liberar y Cerrar
  shutdown(thread_server_cpu, SHUT_RDWR);
  liberar_conexion(socket_km);
  liberar_conexion(socket_server_cpu);
  pthread_join(thread_server_cpu, NULL);
  log_destroy(logger);
  config_destroy(config);
  return EXIT_SUCCESS;
}

void* hilo_escucha_cpu(void* datos_hilo_escucha_void)
{
  int socket_espera_cpu =
      ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->socket_fd;
  t_log* logger = ((t_datos_hilo_escucha*)datos_hilo_escucha_void)->logger;

  while (seguir_operando)
  {
    int socket_cpu = esperar_cliente(socket_espera_cpu);
    if (socket_cpu < 0)
    {
      seguir_operando = false;
      break;
    }
    log_info(logger, "## Conexión exitosa con Kernel Memory");

    // Handshake con CPU
    int id_modulo = recibir_handshake(socket_cpu);
    if (id_modulo != MID_KERNEL_MEMORY)
    {
      log_error(logger, "## Error en el Handshake con CPU");
      continue;
    }
    enviar_handshake(MID_MEMORY_STICK, socket_cpu);
    log_info(logger, "## Handshake exitoso con Kernel Memory");

    char* recibir_id_cpu();
  }
  log_info(logger, "## Cerrando servidor");
  return NULL;
}

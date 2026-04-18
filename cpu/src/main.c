#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>

#include "cpu/cpu.h"
#include "utils/hello.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_cpu* cpu;
  cpu = malloc(sizeof(t_cpu));

  // verifica recibir correctamente los argumentos.(ruta a cpu->confige id)
  if (argc < 3)
  {
    printf("Uso: %s [config] [id]\n", argv[0]);
    return 1;
  }

  char* path_config = argv[1];
  cpu->id = argv[2];

  char* ip_kernel_memory;
  char* puerto_kernel_memory;
  char* ip_kernel_scheduler;
  char* puerto_kernel_scheduler;
  t_log_level log_level;

  // CONFIG Y LOGS
  cpu->config = config_create(path_config);

  log_level =
      log_level_from_string(config_get_string_value(cpu->config, "LOG_LEVEL"));
  cpu->logger = log_create("cpu.log", cpu->id, true, log_level);

  if (cpu->config == NULL)
  {
    log_error(cpu->logger, "No se pudo cargar el config");
    abort();
  }

  log_info(cpu->logger, "Iniciando CPU %s", cpu->id);
  log_info(cpu->logger, "cpu->configcargado correctamente");

  // CONEXION CON EL KERNEL SCHEDULER

  ip_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_IP");
  puerto_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_PUERTO");

  cpu->socket_kernel_scheduler =
      crear_conexion(ip_kernel_scheduler, puerto_kernel_scheduler);

  // Handshake con Kernel scheduler
  enviar_handshake(MID_CPU, cpu->socket_kernel_scheduler);

  int id_modulo = recibir_handshake(cpu->socket_kernel_scheduler);
  if (id_modulo != MID_KERNEL_SCHEDULER)
  {
    log_error(cpu->logger, "## Error en el Handshake con Kernel_scheduler");
    close(cpu->socket_kernel_scheduler);
    log_destroy(cpu->logger);
    config_destroy(cpu->config);
    return EXIT_FAILURE;
  }
  log_info(cpu->logger, "## Handshake exitoso con Kernel scheduler");

  enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_scheduler);


  // CONEXION CON EL KERNEL MEMORY

  ip_kernel_memory = config_get_string_value(cpu->config, "KERNEL_MEMORY_IP");
  puerto_kernel_memory =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_PUERTO");

  cpu->socket_kernel_memory =
      crear_conexion(ip_kernel_memory, puerto_kernel_memory);

  // Handshake con Kernel Memory
  enviar_handshake(MID_CPU, cpu->socket_kernel_memory);

  id_modulo = recibir_handshake(cpu->socket_kernel_memory);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(cpu->logger, "## Error en el Handshake con Kernel_Memory");
    close(cpu->socket_kernel_memory);
    log_destroy(cpu->logger);
    config_destroy(cpu->config);
    return EXIT_FAILURE;
  }
  log_info(cpu->logger, "## Handshake exitoso con Kernel Memory");

  enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_memory);

  // CONEXION CON MEMORY STICK
  // hilo de escucha
  iniciar_hilo(cpu);
  log_info(cpu->logger, "Hilo de escucha de Kernel Memory iniciado");
  escuchar_kernel_memory(cpu);
}
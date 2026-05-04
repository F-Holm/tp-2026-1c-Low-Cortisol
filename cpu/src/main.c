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
  if (!verificar_argumentos(argc, argv))
    return EXIT_FAILURE;

  char* path_config = argv[1];
  cpu->id = argv[2];

  if (!iniciar_modulo(cpu, path_config))
  {
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
  }

  if (!iniciar_conexion_scheduler(cpu))
  {
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
  }

  if (enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "String correctamente enviado al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger, "## fallo el envio del mensaje al kernel scheduler");
  }

  if (!iniciar_conexion_kmemory(cpu))
  {
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
  }

  if (enviar_string(OP_ID_CPU, cpu->id, cpu->socket_kernel_memory))
  {
    log_info(cpu->logger, "String correctamente enviado al kernel memory");
  }
  else
  {
    log_error(cpu->logger, "## fallo el envio del mensaje al kernel memory");
  }

  // CONEXION CON MEMORY STICK
  // hilo de escucha
  iniciar_hilo(cpu);

  pthread_join(cpu->hilos.kernel_memory_hilo, NULL);
  return 0;
}
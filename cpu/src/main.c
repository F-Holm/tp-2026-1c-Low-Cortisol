#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>

#include "cpu/conexiones.h"
#include "cpu/cpu.h"
#include "cpu/inicializador.h"
#include "cpu/liberacion.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_cpu* cpu;
  cpu = malloc(sizeof(t_cpu));

  // verifica recibir correctamente los argumentos
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
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
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
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
  }
  log_info(cpu->logger, "Socket KM: %d", cpu->socket_kernel_memory);
  if (!recibir_tamanio_maximo_segmento(cpu))
  {
    cerrar_modulo(cpu);
    return EXIT_FAILURE;
  }

  // diccionario de intruciones (nombre - funcion)
  cpu->handlers = dictionary_create();
  iniciar_diccionario(cpu->handlers);

  manejo_instrucciones(cpu);

  cerrar_modulo(cpu);

  return 0;
}
#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/io.h"
#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/kernel_scheduler.h"
#include "kernel_scheduler/mutex.h"
#include "kernel_scheduler/queue.h"
#include "kernel_scheduler/server.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_kernel_scheduler_recursos recursos = {0};

  // args
  if (argc != 3)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  char* path_proceso_inicial = argv[2];

  // Iniciar módulo
  if (!iniciar_modulo(&recursos, archivo_config))
  {
    cerrar_modulo_error(&recursos);
    return EXIT_FAILURE;
  }

  // Inicializar datos para el servidor
  inicializar_colas_mutex(&recursos);
  t_datos_servidor_escucha datos;
  inicializar_datos_server_escucha(&datos, recursos->socket_server,
                                   recursos->logger, recursos->lista_mutex,
                                   recursos->colas);

  // Empezar a escuchar servidor
  servidor_escucha(&datos);

  // Liberar y Cerrar
  cerrar_modulo(&recursos);
  return EXIT_SUCCESS;
}

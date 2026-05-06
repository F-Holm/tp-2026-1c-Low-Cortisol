#include <arpa/inet.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/kernel_scheduler.h"
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

  // Esperando Instrucciones del Kernel Memory
  bool seguir_operando = true;
  while (seguir_operando)
  {
    int op_code = recibir_operacion(recursos.socket_km);
    char* buffer;

    switch (op_code)
    {
      case OP_CODE_ERROR:
        seguir_operando = false;
        break;
      default:
        buffer = recibir_string(recursos.socket_km);
        free(buffer);
        break;
    }
  }

  // Liberar y Cerrar
  cerrar_modulo(&recursos);
  return EXIT_SUCCESS;
}

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

int main(int argc, char* argv[])
{
  t_ms_recursos ms_recursos = {0};
  pthread_t thread_server_cpu;

  // args
  char* archivo_config = NULL;
  char* tamanio_str = NULL;
  int tamanio;
  if (!get_args(argc, argv, archivo_config, tamanio_str, &tamanio))
    return EXIT_FAILURE;

  // Iniciar módulo
  if (!iniciar_modulo(&ms_recursos, archivo_config, tamanio_str,
                      &thread_server_cpu))
  {
    cerrar_modulo_error(&ms_recursos);
    return EXIT_FAILURE;
  }

  // Esperando Instrucciones del Kernel Memory
  bool seguir_operando = true;
  while (seguir_operando)
  {
    int op_code = recibir_operacion(ms_recursos.socket_km);
    char* buffer;

    switch (op_code)
    {
      case OP_CODE_ERROR:
        seguir_operando = false;
        break;
      default:
        buffer = recibir_string(ms_recursos.socket_km);
        free(buffer);
        break;
    }
  }

  // Liberar y Cerrar
  cerrar_modulo(&ms_recursos, &thread_server_cpu);
  return EXIT_SUCCESS;
}

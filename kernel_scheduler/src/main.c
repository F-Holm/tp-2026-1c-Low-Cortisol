#include <commons/config.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "kernel_scheduler/cpu.h"
#include "kernel_scheduler/io.h"
#include "kernel_scheduler/kernel_scheduler.h"
#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

int main(int argc, char* argv[])
{
  bool seguir_operando = true;

  t_kScheduler_recursos kScheduler_recursos;
  t_datos_hilo_escucha datos_hilo_escucha;

  if (argc != 3)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  char* path_proceso_inicial = argv[2];

  iniciar_modulo(&kScheduler_recursos, archivo_config);

  // conectar con kernel memory como cliente y loggear el resultado
  if(conectar_kernel_memory(&kScheduler_recursos) == false) return EXIT_FAILURE;
  //PREGUNTARLE A HOLM SI ESTO ANDA <3

  // handshake con kernel memory y loggear el resultado
  if(handshake_kernel_memory(&kScheduler_recursos) == false) return EXIT_FAILURE;
  //PREGUNTARLE A HOLM SI ESTO ANDA <3

  //----------------------------------------------------------------------------------

  // iniciar servidor para CPU y IO
  kScheduler_recursos.server = iniciar_servidor(kScheduler_recursos.puerto_servidor);

  // creacion de hilos
  iniciar_servidor_cpu_io(&kScheduler_recursos, &datos_hilo_escucha);
  // recibir operaciones de CPU y IO
  while (seguir_operando)
  {
    int op_code = recibir_operacion(kScheduler_recursos.socket_km);
    switch (op_code)
    {
        // agregar casos para cada operacion que se quiera recibir de la CPU y
        // IO
      case OP_CODE_ERROR:
        log_error(kScheduler_recursos.logger, "## Se termino la conexion con el servidor.");
        seguir_operando = false;
        break;
      default:
        log_warning(kScheduler_recursos.logger, "## Operacion desconocida.");
        break;
    }
  }

  // cerrar servidor y liberar recursos
  cerrar_modulo(&kScheduler_recursos);
  return EXIT_SUCCESS;
}
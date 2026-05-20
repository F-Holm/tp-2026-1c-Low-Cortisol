#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "io/ioops.h"
#include "io/utils.h"
#include "utils/client.h"
#include "utils/hello.h"
#include "utils/io.h"
#include "utils/msg.h"
int main(int argc, char* argv[])
{
  t_modulo_io sio;
  t_peticion_stdin peticion_stdin;

  if (!args(argc, argv, &sio))
  {
    return EXIT_FAILURE;
  }

  if (!cargar_configs(&sio))
  {
    return EXIT_FAILURE;
  }
  if (!iniciar_enviar_tipo_io(&sio))
  {
    return EXIT_FAILURE;
  }
  // Esperando Instrucciones del Kernel Scheduler
  bool seguir_operando;
  while (seguir_operando)
  {
    int op_code;
    op_code = recibir_operacion(sio.socket_io);
    switch(op_code)
    {
      case OP_PETICION_IO_STDIN:
      if (!io_tipo_stdin){
        seguir_operando = false;
      }
      break;
    
      case OP_PETICION_IO_STDOUT:
      if(!io_tipo_stdout){
        seguir_operando = false;
      }
      break;

      case OP_PETICION_IO_SLEEP:
      if(!io_tipo_sleep){
        seguir_operando = false;
      }
      break;

      default:
      seguir_operando = false;
  }
    
  }
  // Liberar y Cerrar
  cerrar_todo(&sio);
  free(sio.input);
  return EXIT_SUCCESS;
}

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "io/ioops.h"
#include "io/utils.h"
#include "utils/config.h"
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_modulo_io sio;

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
  bool seguir_operando = true;
  bool operacion = -1;
  while (seguir_operando)
  {
    int op_code;
    op_code = recibir_operacion(sio.socket_io);
    switch (op_code)
    {
      case OP_PETICION_IO_STDIN:
        operacion = io_tipo_stdin(&sio);
        if (!operacion)
        {
          seguir_operando = false;
        }
        break;

      case OP_PETICION_IO_STDOUT:
        operacion = io_tipo_stdout(&sio);
        if (!operacion)
        {
          seguir_operando = false;
        }
        break;

      case OP_PETICION_IO_SLEEP:
        operacion = io_tipo_sleep(&sio);
        if (!operacion)
        {
          seguir_operando = false;
        }
        break;

      default:
        seguir_operando = false;
    }
  }
  log_info(sio.logger, " Cierre de IO");
  // Liberar y Cerrar
  cerrar_todo(&sio);
  return EXIT_SUCCESS;
}

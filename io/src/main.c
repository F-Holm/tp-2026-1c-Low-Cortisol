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
  t_io sio;

  if (!parse_args(argc, argv, &sio))
  {
    return EXIT_FAILURE;
  }

  if (!load_config(&sio))
  {
    return EXIT_FAILURE;
  }
  if (!connect_to_scheduler(&sio))
  {
    return EXIT_FAILURE;
  }
  // Esperando Instrucciones del Kernel Scheduler
  bool seguir_operando = true;
  bool operacion = -1;
  while (seguir_operando)
  {
    int op_code;
    op_code = receive_op_code(sio.socket_io);
    switch (op_code)
    {
      case OP_IO_STDIN_REQUEST:
        operacion = run_stdin(&sio);
        if (!operacion)
        {
          seguir_operando = false;
        }
        break;

      case OP_IO_STDOUT_REQUEST:
        operacion = run_stdout(&sio);
        if (!operacion)
        {
          seguir_operando = false;
        }
        break;

      case OP_IO_SLEEP_REQUEST:
        operacion = run_sleep(&sio);
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
  close_io(&sio);
  return EXIT_SUCCESS;
}

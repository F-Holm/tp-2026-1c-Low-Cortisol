#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "io/compactio.h"
#include "utils/client.h"
#include "utils/hello.h"
#include "utils/io.h"
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
  while (true)
  {
    int op_code = recibir_operacion(sio.socket_io);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(sio.socket_io);
    free(buffer);
  }
  // Liberar y Cerrar
  cerrar_todo(&sio);
  return EXIT_SUCCESS;
}

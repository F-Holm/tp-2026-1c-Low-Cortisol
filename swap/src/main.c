#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "swap/agregados.h"
#include "utils/client.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_modulo_swap sswap;

  if (argc != 2)
    return EXIT_FAILURE;
  char* archivo_config = argv[1];
  sswap.config = config_create(archivo_config);
  if (sswap.config == NULL)
    return EXIT_FAILURE;

  if (!inicializar_configuracion(&sswap))
  {
    return EXIT_FAILURE;
  }

  if (!iniciar_conexion(&sswap))
  {
    return EXIT_FAILURE;
  }
  // Esperando Instrucciones del Kernel Scheduler
  while (true)
  {
    int op_code = recibir_operacion(sswap.socket_swap);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(sswap.socket_swap);
    free(buffer);
  }
  // Liberar y Cerrar
  cerrar_todo(&sswap);
  return EXIT_SUCCESS;
}

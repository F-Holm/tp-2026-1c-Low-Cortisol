#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "swap/agregados.h"
#include "utils/client.h"
#include "utils/hello.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_config* config;
  int socket_swap;
  char* puerto;
  char* ip;
  char* log_levelstr;
  // Args
  if (argc != 2)
    return EXIT_FAILURE;

  char* archivo_config = argv[1];
  // Crea config
  config = config_create(archivo_config);
  if (config == NULL)
    return EXIT_FAILURE;

  log_levelstr = config_get_string_value(config, "LOG_LEVEL");

  t_log_level log_level = log_level_from_string(log_levelstr);

  t_log* logger =
      log_create("swap.log", "SWAP", true,
                 log_level);  // inicio el log para poder enviar los logs
  if (logger == NULL)
  {
    config_destroy(config);
    return EXIT_FAILURE;
  }
  ip = config_get_string_value(config,
                               "IP");  // Obtengo la IP del archivo de configs

  puerto = config_get_string_value(config, "PORT");  // Obtengo el puerto

  socket_swap = crear_conexion(ip, puerto);  // Establezco conexión

  if (socket_swap == -1)
  {
    log_error(logger, "#ERROR DE CONEXION");  // Informo error
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;  // Termino el programa
  }
  log_info(logger,
           "## Conectado a Kernel Memory");  // Loggeo el comentario de
                                             // conexion iniciada

  // Handshake con kernel memory
  bool envio_correcto = enviar_handshake(MID_SWAP, socket_swap);
  if (!envio_correcto)
  {
    log_error(logger, "## Error en el Handshake con Kernel Memory");
    cerrar_todo(logger, config, socket_swap);
    return EXIT_FAILURE;
  }
  // Chequea error en el envio

  bool recepcion_correcta = recibir_handshake(socket_swap);
  if (!recepcion_correcta)
  {
    log_error(logger, "## Error en el Handshake con Kernel Memory");
    cerrar_todo(logger, config, socket_swap);
    return EXIT_FAILURE;
  }
  // Chequea error en la recepción
  log_info(logger, "## Handshake exitoso con Kernel Memory");

  // Esperando Instrucciones del Kernel Memory
  while (true)
  {
    int op_code = recibir_operacion(socket_swap);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(socket_swap);
    free(buffer);
  }

  // Liberar y Cerrar
  cerrar_todo(logger, config, socket_swap);
  return EXIT_SUCCESS;
}

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
  t_config* config;
  int socket_io;
  char* puerto;
  char* ip;
  char* log_levelstr;
  int tipo_io;
  t_log_level log_level;

  if (argc != 3)
    return EXIT_FAILURE;

  // Argumentos
  char* archivo_config = argv[1];

  // Chequeo si la operacion de IO recibida existe
  if (strcmp(V_TIPO_IO[E_STDIN], argv[2]) == 0)
    tipo_io = E_STDIN;
  else if (strcmp(V_TIPO_IO[E_STDOUT], argv[2]) == 0)
    tipo_io = E_STDOUT;
  else if (strcmp(V_TIPO_IO[E_SLEEP], argv[2]) == 0)
    tipo_io = E_SLEEP;
  else
    return EXIT_FAILURE;

  config = config_create(archivo_config);
  if (config == NULL)
    return EXIT_FAILURE;

  log_levelstr = config_get_string_value(config, "LOG_LEVEL");

  log_level = log_level_from_string(log_levelstr);

  t_log* logger = log_create("io.log", "io", true, log_level);
  // inicio el log para poder enviar los logs
  if (logger == NULL)
  {
    config_destroy(config);
    return EXIT_FAILURE;
  };
  ip = config_get_string_value(config, "IP");
  // Obtengo la IP del archivo de configs

  puerto = config_get_string_value(config, "PORT");  // Obtengo el puerto

  socket_io = crear_conexion(ip, puerto);  // Establezco conexión

  if (socket_io == -1)
  {
    log_error(logger, "#ERROR DE CONEXION");  // Informo error
    cerrar_todo(logger, config, socket_io);
    return EXIT_FAILURE;  // Termino el programa
  }
  log_info(logger,
           "## Conectado a Kernel Scheduler");  // Loggeo el comentario de
                                                // conexion iniciada

  // Handshake con Kernel Scheduler
  bool envio_correcto = enviar_handshake(MID_IO, socket_io);
  if (!envio_correcto)
  {
    log_error(logger, "## Error en el Handshake con Kernel Scheduler");
    cerrar_todo(logger, config, socket_io);
    return EXIT_FAILURE;
  }

  bool recepcion_correcta = recibir_handshake(socket_io);
  if (!recepcion_correcta)
  {
    log_error(logger, "## Error en el Handshake con Kernel Scheduler");
    cerrar_todo(logger, config, socket_io);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Handshake exitoso con Kernel Scheduler");

  enviar_string(OP_TIPO_IO, (char*)V_TIPO_IO[tipo_io], socket_io);
  log_info(logger, "## Envio tipo de IO");

  // Esperando Instrucciones del Kernel Scheduler
  while (true)
  {
    int op_code = recibir_operacion(socket_io);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(socket_io);
    free(buffer);
  }
  // Liberar y Cerrar
  cerrar_todo(logger, config, socket_io);
  return EXIT_SUCCESS;
}

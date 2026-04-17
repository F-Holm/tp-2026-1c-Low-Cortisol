#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "utils/client.h"
#include "utils/hello.h"
#include "utils/io.h"
#include "utils/msg.h"
int main(int argc, char* argv[])
{
  t_config* config;
  int socket_IO;
  char* puerto;
  char* ip;
  char* log_levelstr;
  int tipo_io;

      if (argc != 3)
  {
    return EXIT_FAILURE;
  }
  // Argumentos
  char* archivo_config = argv[1];

  // Chequeo si la operacion de IO recibida existe
  if (strcmp(v_tipo_io[E_STDIN], argv[2]) == 0)
  {
    tipo_io = E_STDIN;
  }
  else if (strcmp(v_tipo_io[E_STDOUT], argv[2]) == 0)
  {
    tipo_io = E_STDOUT;
  }
  else if (strcmp(v_tipo_io[E_SLEEP], argv[2]) == 0)
  {
    tipo_io = E_SLEEP;
  }
  else
  {
    return EXIT_FAILURE;
  }

  config = config_create(archivo_config);
  if (config == NULL)
  {
    return EXIT_FAILURE;
  };

  log_levelstr = config_get_string_value(config, "LOG_LEVEL");

  t_log_level log_level = log_level_from_string(log_levelstr);

  t_log* logger =
      log_create("io.log", "io", true,
                 log_level);  // inicio el log para poder enviar los logs
  if (logger == NULL)
  {
    config_destroy(config);
    return EXIT_FAILURE;
  };
  ip = config_get_string_value(config,
                               "IP");  // Obtengo la IP del archivo de configs

  puerto = config_get_string_value(config, "PORT");  // Obtengo el puerto

  socket_IO = crear_conexion(ip, puerto);  // Establezco conexión

  if (socket_IO == -1)
  {
    log_error(logger, "#ERROR DE CONEXION");  // Informo error
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;  // Termino el programa
  }
  else
  {
    log_info(logger,
             "## Conectado a Kernel Scheduler");  // Loggeo el comentario de
                                                  // conexion iniciada
  };

  // Handshake con Kernel Scheduler
  enviar_handshake(MID_IO, socket_IO);
  int id_modulo = recibir_handshake(socket_IO);
  if (id_modulo != MID_IO)
  {
    log_error(logger, "## Error en el Handshake con Kernel Scheduler");
    close(socket_IO);
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Handshake exitoso con Kernel Scheduler");

  // Esperando Instrucciones del Kernel Scheduler
  while (true)
  {
    int op_code = recibir_operacion(socket_IO);
    char* buffer;

    if (op_code == OP_CODE_ERROR || op_code == -1)
      break;

    buffer = recibir_string(socket_IO);
    free(buffer);
  };
  // Liberar y Cerrar
  liberar_conexion(socket_IO);
  log_destroy(logger);
  config_destroy(config);
  return EXIT_SUCCESS;
}

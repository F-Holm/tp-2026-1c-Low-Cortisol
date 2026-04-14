#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "utils-client.h"
#include "utils/hello.h"

int main(int argc, char* argv[])
{
  t_config config;
  int socket_swap;
  char* puerto;
  char* ip;
  char* log_levelstr;

  t_config* config_create("config.conf");

  char* log_levelstr = config_get_string_value(t_config, "LOG_LEVEL");

  t_log_level log_level = log_level_from_string(log_levelstr);

  t_log* logger =
      log_create("swap.log", "SWAP", true,
                 log_level);  // inicio el log para poder enviar los logs

  ip = config_get_string_value(t_config,
                               "IP");  // Obtengo la IP del archivo de configs

  puerto = config_get_string_value(t_congig, "PORT");  // Obtengo el puerto

  socket_swap = crear_conexion(ip, puerto);  // Establezco conexión

  if (socket_swap == -1)
  {
    log_info("swap.log", "#ERROR DE CONEXION");  // Informo error

    exit(EXIT_FAILURE);  // Termino el programa
  }
  else
  {
    log_info("swap.log",
             "## Conectado a Kernel Memory");  // Loggeo el comentario de
                                               // conexion iniciada
  };
  saludar("swap");
  return 0;
}

#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "utils/client.h"
#include "utils/hello.h"
#include "utils/msg.h"

int main()
{
  t_config* config;
  int socket_IO;
  char* puerto;
  char* ip;
  char* log_levelstr;

  config = config_create("IOconf.conf");

  log_levelstr = config_get_string_value(config, "LOG_LEVEL");

  t_log_level log_level = log_level_from_string(log_levelstr);

  t_log* logger =
      log_create("IO.log", "IO", true,
                 log_level);  // inicio el log para poder enviar los logs

  ip = config_get_string_value(config,
                               "IP");  // Obtengo la IP del archivo de configs

  puerto = config_get_string_value(config, "PORT");  // Obtengo el puerto

  socket_IO = crear_conexion(ip, puerto);  // Establezco conexión

  if (socket_IO == -1)
  {
    log_error(logger, "#ERROR DE CONEXION");  // Informo error
    log_destroy (logger);
    config_destroy (config);
    return EXIT_FAILURE;  // Termino el programa
  }
  else
  {
    log_info(logger,
             "## Conectado a Kernel Scheduler");  // Loggeo el comentario de
                                                  // conexion iniciada
  };
  
  //Handshake con kernel scheduler 
  enviar_handshake (MID_IO,socket_IO);
  int id_modulo = recibir_handshake(socket_IO);
  if(id_modulo != MID_IO){
    log_error(logger, "## Error en el Handshake con Kernel Scheduler");
    close(socket_IO);
    log_destroy(logger);
    config_destroy(config);
    return EXIT_FAILURE;
  }
  log_info(logger, "## Handshake exitoso con Kernel Scheduler");

  
  
  
  
  
  
  return 0;





}

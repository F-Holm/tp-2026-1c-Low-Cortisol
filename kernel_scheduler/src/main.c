#include "../../utils/src/utils/hello.h"
#include "main.h"
#include "../../utils/src/utils/utils-client.h"
#include "../../utils/src/utils/utils-server.h"
#include <commons/log.h>
#include <commons/config.h>
#include <string.h>


int main(int argc, char* argv[])
{
  int conexion;
	char* ip;
	char* puerto;
  char* valor;


	t_log* logger;
	t_config* config;

  //borrar esto despues de probar
  saludar("kernel_scheduler");
  
  config = iniciar_config();
	logger = iniciar_logger(config);
  
  ip = config_get_string_value(config, "IP");
  puerto = config_get_string_value(config, "PUERTO");
  valor = "Hola, soy el kernel scheduler";


  //conectar con kernel memory como cliente y loggear el resultado
  conexion = crear_conexion(ip, puerto);
  if (conexion == -1) {
    log_error(logger, "Fallo la conexion con kernel memory en %s:%s", ip, puerto);
    terminar_programa(conexion, logger, config);
    return 1;
  } else {
    log_info(logger, "Conexion establecida con kernel memory en %s:%s", ip, puerto);
  }
  enviar_mensaje(valor, conexion);
  paquete(conexion, valor);
  //evaluar meter en una funcion lineas 33 a 42

  //iniciar servidor para cpu y io


  return 0;
}

t_log* iniciar_logger(t_config* config)
{
	t_log* nuevo_logger;
  char* log_levelstr = config_get_string_value(config, "LOG_LEVEL");
  t_log_level log_level = log_level_from_string(log_levelstr);
	nuevo_logger = log_create("kernel_scheduler.log", "kernelScheduler", true, log_level);
	return nuevo_logger;
}

t_config* iniciar_config(void)
{
	t_config* nuevo_config;
	nuevo_config = config_create("kernel_scheduler.config");
	return nuevo_config;
}

void paquete(int conexion, char* valor)
{
  t_paquete* paquete = crear_paquete();
  agregar_a_paquete(paquete, valor, strlen(valor));
  enviar_paquete(paquete, conexion);
	eliminar_paquete(paquete);
}

void terminar_programa(int conexion, t_log* logger, t_config* config)
{
	log_destroy(logger);
	config_destroy(config);
	liberar_conexion(conexion);
}
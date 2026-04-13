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
  
	logger = iniciar_logger();
  config = iniciar_config();

  ip = config_get_string_value(config, "IP");
  puerto = config_get_string_value(config, "PUERTO");
  valor = "Hola, soy el kernel scheduler";

  //conectar con kernel memory como cliente
  conexion = crear_conexion(ip, puerto);
  enviar_mensaje(valor, conexion);
  paquete(conexion, valor);
  //falta hacer el log de conexion


  //iniciar servidor para cpu y io


  return 0;
}

t_log* iniciar_logger(void)
{
	t_log* nuevo_logger;
	nuevo_logger = log_create("tp0.log", "logsTP0", true, LOG_LEVEL_INFO);
	return nuevo_logger;
}
t_config* iniciar_config(void)
{
	t_config* nuevo_config;
	nuevo_config = config_create("cliente.config");
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
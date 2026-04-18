#include <commons/config.h>
#include <commons/log.h>
#include <string.h>

#include "utils/client.h"
#include "utils/msg.h"
#include "utils/server.h"

t_log* logger;
t_config* config;

t_log* iniciar_logger(t_config* config)
{
  t_log* nuevo_logger;
  char* log_levelstr = config_get_string_value(config, "LOG_LEVEL");
  t_log_level log_level = log_level_from_string(log_levelstr);
  nuevo_logger =
      log_create("kernel_scheduler.log", "kernelScheduler", true, log_level);
  return nuevo_logger;
}

t_config* iniciar_config(void)
{
  t_config* nuevo_config;
  nuevo_config = config_create("kernel_scheduler.config");
  return nuevo_config;
}

void paquete(int socket_km, char* valor)
{
  t_paquete* paquete = crear_paquete();
  agregar_a_paquete(paquete, valor, strlen(valor));
  enviar_paquete(paquete, socket_km);
  eliminar_paquete(paquete);
}

void terminar_programa(int socket_km, t_log* logger, t_config* config)
{
  log_destroy(logger);
  config_destroy(config);
  liberar_conexion(socket_km);
}
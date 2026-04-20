#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>

typedef struct
{
  char* log_level;
  int memory_delay;
  char* ip_km;
  char* puerto_km;
} t_config_vars;

typedef struct
{
  t_config* config;
  t_log* logger;
  int socket_km;
  int socket_server_cpu;
} t_ms_recursos;

bool conseguir_y_enviar_puerto(int socket_km, int socket_server_cpu,
                               t_log* logger);
bool iniciar_modulo(t_ms_recursos* ms_recursos, char* archivo_config,
                    char* tamanio);
t_config* iniciar_config(char* archivo_config, t_config_vars* config_vars);
t_log* iniciar_logger(t_log_level log_level);
void read_confir_ms(t_config* config, t_config_vars* config_vars);

#endif /* MEMORY_STICK_MEMORY_STICK_H_ */

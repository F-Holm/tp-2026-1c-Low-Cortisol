#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <pthread.h>
#include <stdio.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/logger.h"

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
  t_logger* logger;
  int memory_delay;
  int socket_km;
  int socket_server_cpu;
  char* memoria;
  pthread_mutex_t* mutex_memoria;
} t_ms_recursos;

bool conseguir_y_enviar_puerto(int socket_km, int socket_server_cpu,
                               t_logger* logger);
bool iniciar_modulo(t_ms_recursos* ms_recursos, char* archivo_config,
                    char* tamanio, pthread_t* hilo_server_cpu);
t_config* iniciar_config(char* archivo_config, t_config_vars* config_vars);
t_logger* iniciar_logger(t_log_level log_level);
void read_confir_ms(t_config* config, t_config_vars* config_vars);
void cerrar_modulo_error(t_ms_recursos* ms_recursos);
void cerrar_modulo(t_ms_recursos* ms_recursos, pthread_t* thread_server_cpu);
bool get_args(int argc, char** argv, char** archivo_config, char** tamanio_str,
              int* tamanio);
void escribir_memoria(t_ms_recursos* ms_recursos, int posicion_inicial,
                      char* bytes_a_escribir, int cantidad_de_bytes,
                      int socket_destino);
void leer_memoria(t_ms_recursos* ms_recursos, int posicion_inicial,
                  int cantidad_de_bytes, int socket_destino);
#endif /* MEMORY_STICK_MEMORY_STICK_H_ */

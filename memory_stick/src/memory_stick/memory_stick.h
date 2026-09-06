#pragma once

#include <pthread.h>
#include <stdio.h>

#include "utils/config.h"
#include "utils/log.h"

typedef struct
{
  char* log_level;
  int memory_delay;
  char* km_ip;
  char* km_port;
} t_config_vars;

typedef struct
{
  t_config* config;
  t_log* logger;
  int memory_delay;
  int socket_km;
  int socket_server_cpu;
  char* memory;
  pthread_mutex_t* memory_mutex;
} t_ms;

bool send_cpu_server_port(int socket_km, int socket_server_cpu, t_log* logger);
bool init_module(t_ms* ms, char* config_path, char* size,
                 pthread_t* cpu_server_thread);
t_config* init_config(char* config_path, t_config_vars* config_vars);
t_log* init_logger(t_log_level log_level);
void read_config(t_config* config, t_config_vars* config_vars);
void close_module_on_error(t_ms* ms);
void close_module(t_ms* ms, pthread_t* cpu_server_thread);
bool get_args(int argc, char** argv, char** config_path, char** size_str,
              int* size);
void write_memory(t_ms* ms, int start_position, char* bytes_to_write,
                  int byte_count, int dest_socket);
void read_memory(t_ms* ms, int start_position, int byte_count, int dest_socket);

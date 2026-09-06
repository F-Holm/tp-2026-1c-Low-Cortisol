#pragma once

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/mutex.h"
#include "kernel_scheduler/queue.h"
#include "utils/log.h"
#include "utils/logger.h"

typedef struct
{
  int socket_server;
  t_logger* logger;
  t_lista_mutex* lista_mutex;
  t_colas* colas;
  t_socket_kernel_memory* socket_km;
  char* path_proceso_inicial;
} t_datos_servidor_escucha;

int crear_socket_servidor(char* puerto, t_logger* logger);
void inicializar_datos_server_escucha(
    t_datos_servidor_escucha* datos, int socket_server, t_logger* logger,
    t_lista_mutex* lista_mutex, t_colas* colas,
    t_socket_kernel_memory* socket_kernel_memory, char* path_proceso_inicial);
void servidor_escucha(t_datos_servidor_escucha* datos);

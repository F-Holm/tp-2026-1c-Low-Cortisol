#pragma once

#include "kernel_scheduler/misc.h"
#include "utils/log.h"

typedef struct
{
  int socket_servidor;
  t_log* logger;
  t_socket_kernel_memory* socket_km;
  bool cerrar;
  pthread_mutex_t mutex_cerrar;
  pthread_t hilo;
} t_datos_hilo_verificar_conexion;

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_log* logger);
bool avisar_terminar_proceso(t_socket_kernel_memory* socket_km, uint32_t pid,
                             int socket_servidor, t_log* logger);
t_datos_hilo_verificar_conexion* iniciar_hilo_verificar_conexion_kernel_memory(
    int socket_servidor, t_log* logger, t_socket_kernel_memory* socket_km);
void destruir_hilo_verificar_conexion_kernel_memory(
    t_datos_hilo_verificar_conexion* datos);

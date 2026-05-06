#ifndef KERNEL_SCHEDULER_KERNEL_SCHEDULER_H_
#define ERNEL_SCHEDULER_KERNEL_SCHEDULER_H_

#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_algoritmo_planificacion;

const char* const ALGORITMOS_PLANIFICACION[3];

typedef struct
{
  t_log_level log_level;
  int algoritmo_planificacion;
  t_list* algoritmos_cmn;
  int rr_quantum;
  bool desalojo;
  int suspension_timeout;
  char* puerto_servidor;
  char* ip_kernel_memory;
  char* puerto_kernel_memory;
} t_config_vars;

typedef struct
{
  int socket_kernel_memory;
  int socket_server;
  pthread_t hilo_servidor;
  t_config* config;
  t_log* logger;
  t_config_vars config_vars;
} t_kernel_scheduler_recursos;

bool iniciar_modulo(t_kernel_scheduler_recursos* recursos,
                    char* archivo_config);

#endif
#ifndef KERNEL_SCHEDULER_KERNEL_SCHEDULER_H_
#define ERNEL_SCHEDULER_KERNEL_SCHEDULER_H_

#include <commons/config.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils/registros.h"

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_algoritmo_planificacion;

typedef enum
{
  NEW,
  READY,
  EXEC,
  BLOCK,
  SUSP_BLOCK,
  SUSP_READY,
  EXIT
} t_estado;

extern const char* const ESTADO_PROCESO[7];
extern const char* const ALGORITMOS_PLANIFICACION[3];

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

typedef struct
{
  uint32_t pid;
  uint32_t ppid;
  t_estado estado;
  t_contexto contexto; // Falta definir la estructura del contexto (cpu)

}

bool iniciar_modulo(t_kernel_scheduler_recursos* recursos,
                    char* archivo_config);
void cerrar_modulo_error(t_kernel_scheduler_recursos* recursos);
void cerrar_modulo(t_kernel_scheduler_recursos* recursos);

#endif
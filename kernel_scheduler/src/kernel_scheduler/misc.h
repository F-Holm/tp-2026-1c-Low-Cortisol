#ifndef KERNEL_SCHEDULER_MISC_H_
#define KERNEL_SCHEDULER_MISC_H_

#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/logger.h"

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_algoritmo_planificacion;

typedef struct
{
  uint32_t pid;
  int prioridad;
  pthread_mutex_t mutex_pcb;
  unsigned long tiempo_bloqueado;
} t_pcb;

typedef struct
{
  int socket_km;
  pthread_mutex_t mutex_socket;
} t_socket_kernel_memory;

typedef struct
{
  int cantidad_procesos_activos;
  pthread_mutex_t mutex_contador;
  int socket_servidor;
  t_logger* logger;
} t_contador_procesos;

typedef enum
{
  MC_SIN_PROCESOS,
  MC_MEMORIA_CORRUPTA,
  MC_FALLO_CONEXION_KERNEL_MEMORY,
  MC_CAUSA_DESCONOCIDA
} t_motivo_cierre;

extern const char* const MOTIVOS_CIERE[4];

typedef enum
{
  D_ERROR_KM,
  D_ERROR_IO,
  D_ERROR_CONEXION_KM,
  D_TODO_BIEN
} devolucion_syscall;

void cerrar_kernel_scheduler(int socket_servidor, t_logger* logger,
                             int motivo_cierre);

t_socket_kernel_memory* inicializar_socket_kernel_memory(int socket_km);
void destruir_kernel_memory(t_socket_kernel_memory* socket_km);
// retorna el indice del elemento ingresado
int insertar_pcb_en_orden(t_list* lista, t_pcb* pcb);
int get_prioridad_pcb(t_pcb* pcb);
t_pcb* crear_pcb(void);
void destruir_pcb(t_pcb* pcb);
bool responder_handshake(int socket_fd, int id_modulo, t_logger* logger);
unsigned long millis(void);
unsigned long time_diff(unsigned long time_1, unsigned long time_2);
t_contador_procesos* inicializar_contador_procesos(int socket_servidor,
                                                   t_logger* logger);
void aumentar_contador_procesos(t_contador_procesos* contador);
void disminuir_contador_procesos(t_contador_procesos* contador);
void destruir_contador_procesos(t_contador_procesos* contador);

#endif /* KERNEL_SCHEDULER_MISC_H_ */

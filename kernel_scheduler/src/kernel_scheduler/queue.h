#ifndef KERNEL_SCHEDULER_QUEUE_H_
#define KERNEL_SCHEDULER_QUEUE_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/misc.h"
#include "utils/logger.h"

typedef enum
{
  MFP_PRIORIDAD_NO_VALIDA,
  MFP_INSTRUCCION_EXIT,
  MFP_CIERRE_SISTEMA,
  MFP_FALLO_IO,
  MPF_MEMORIA_INSUFICIENTE
} t_motivos_fin_proceso;

extern const char* const MOTIVOS_FIN_PROCESO[5];

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
  pthread_cond_t cond_nuevo_proceso;
} t_lista;

typedef struct
{
  t_queue* cola;
  int algoritmo;
} t_cola_individual_ready;

typedef struct
{
  int cantidad_colas;
  t_cola_individual_ready* colas;
  bool cola_multi_nivel;
  pthread_mutex_t mutex_cola;
  int cant_procesos_ready;
  pthread_cond_t nuevo_proceso;
  pthread_cond_t salida_desbloqueada;
  pthread_mutex_t bloquear_salida;
  bool desalojar_todo;
  int mayor_prioridad;
  pthread_mutex_t mutex_desalojo_prioritario;
  pthread_cond_t cola_vacia;
} t_cola_ready;

typedef struct
{
  t_list* lista;
  pthread_mutex_t mutex_lista;
  t_pcb* prioridad_mas_baja;
  int quantum;      // = 0 si no es RR
  bool desalojo;    // Si el desalojo está habilitado
} t_lista_execute;  // Como algunos valores no cambian nunca (quantum y
                    // desalojo), no necesitan mutex

typedef enum
{
  EH_EJECUTANDO,
  EH_ESPERANDO_PROCESO,
  EH_BLOQUEADO,
  EH_FINALIZANDO,
  EH_FINALIZADO
} t_estado_hilo;

typedef struct
{
  pthread_t hilo;
  pthread_mutex_t mutex_estado;
  int estado;
  pthread_cond_t* esperar_proceso;
  pthread_cond_t desbloquear;
} t_datos_hilo_suspendido;

typedef struct
{
  t_datos_hilo_suspendido* datos;
  int suspension_timeout;
} t_datos_hilo_suspensor;

typedef struct
{
  t_datos_hilo_suspendido* datos;
} t_datos_hilo_des_suspensor;

typedef struct
{
  t_datos_hilo_suspensor* datos_hilo_suspensor;
  t_datos_hilo_des_suspensor* datos_hilo_des_suspensor;
} t_datos_suspendido;

typedef struct
{
  t_cola_ready ready;
  t_lista_execute exec;
  t_lista block;
  t_lista susp_block;
  t_lista susp_ready;
  t_contador_procesos* contador_procesos;
  t_logger* logger;
  t_socket_kernel_memory* socket_km;
  int socket_servidor;
  t_datos_suspendido* datos_suspendido;
  pthread_mutex_t mutex_rutina;
  bool terminar_rutinas;
} t_colas;

// ingresar NULL en t_list si no es CMN
// ingresar quantum = 0 si no es RR
t_colas* inicializar_colas(int algoritmo, t_list* algoritmos_cmn, int quantum,
                           bool desalojo, int socket_servidor, t_logger* logger,
                           t_socket_kernel_memory* socket_km,
                           int suspension_timeout);
void destruir_colas(t_colas* colas);

bool esta_cola_ready_bloqueada(t_cola_ready* ready);
void bloquear_cola_ready(t_cola_ready* ready);
void desbloquear_cola_ready(t_cola_ready* ready);
void esperar_cola_ready_vacia(t_cola_ready* ready);
bool puedo_suspender(t_pcb* pcb, int suspension_timeout);

// cambio_ready_exec: No implementado, solo contiene el log por ahora. Usar
// funciones individuales
void cambio_a_exec(t_pcb* pcb, t_lista_execute* exec);
t_pcb* cambio_sacar_ready(t_cola_ready* ready);
t_pcb* cambio_sacar_ready_bloqueante(t_cola_ready* ready);
void cambio_ready_exec(t_pcb* pcb, t_colas* colas);

// Funciones de cambios de estados
void cambio_new_ready(t_colas* colas, char* archivo_instrucciones,
                      int prioridad);
void cambio_exec_ready(t_pcb* pcb, t_colas* colas);
void cambio_exec_exit(t_pcb* pcb, t_colas* colas, int motivo);
void cambio_exec_block(t_pcb* pcb, t_colas* colas);
void cambio_block_ready(t_pcb* pcb, t_colas* colas);
void cambio_block_susp_block(t_pcb* pcb, t_colas* colas);
void cambio_susp_block_block(t_pcb* pcb, t_colas* colas);
void cambio_susp_block_susp_ready(t_pcb* pcb, t_colas* colas);
void cambio_susp_ready_ready(t_pcb* pcb, t_colas* colas);
void cambio_desbloquear(t_pcb* pcb, t_colas* colas);

// Para errores o rutinas de cierre
void vaciar_colas(t_colas* colas);

// Para bloquear y desbloquear los hilos suspensor y des-suspensor
void bloquear_hilos_suspendido(t_colas* colas);
void desbloquear_hilos_suspendido(t_colas* colas);

// Funciones de consultas a kernel_memory
int espacio_disponible(t_colas* colas, uint32_t pid);
int espacio_disponible_sin_mutex(t_colas* colas, uint32_t pid);
int tamanio_proceso(t_colas* colas, uint32_t pid);
int tamanio_proceso_sin_mutex(t_colas* colas, uint32_t pid);

// Funciones de rutinas
void crear_hilo_rutina_des_suspension(t_colas* colas);
void crear_hilo_compactacion(t_colas* colas);

#endif /* KERNEL_SCHEDULER_QUEUE_H_ */

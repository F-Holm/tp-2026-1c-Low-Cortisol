#ifndef KERNEL_SCHEDULER_IO_H_
#define KERNEL_SCHEDULER_IO_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/kernel_scheduler.h"

/****************** FUNCIONES DE IO ******************/
typedef struct
{
  int socket_io;
  
  pthread_cond_t condicion_fin;
  t_pcb* proceso_actual;
  bool prioridad_activa;
  pthread_cond_t nuevo_proceso;
  t_cola_ready* cola_ready;
  t_cola* cola_block;
  t_lista* susp_block;
  t_lista* susp_ready;
  t_logger* logger;
  t_socket_kernel_memory* socket_km;
} t_io;

typedef struct
{
  t_pcb* pcb;
  t_peticion_stdin* peticion;
} t_stdin;

typedef struct
{
  t_pcb* pcb;
  t_peticion_stdout* peticion;
} t_stdout;

typedef struct
{
  t_pcb* pcb;
  t_peticion_sleep* peticion;
} t_sleep;

typedef struct
{
  t_list* lista_stdin;
  phtread_mutex_t mutex_lista_stdin;
} t_lista_stdin;

typedef struct
{
  t_list* lista_stdout;
  phtread_mutex_t mutex_lista_stdout;
} t_lista_stdout;

typedef struct
{
  t_list* lista_sleep;
  phtread_mutex_t mutex_lista_sleep;
} t_lista_sleep;

typedef struct
{
  t_lista_stdin* lista_peticion_stdin;
  t_lista_stdout* lista_peticion_stdout;
  t_lista_sleep* lista_peticion_sleep;
} listas_peticion_io;

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger);
void cerrar_io(int sockets_io[3]);
int obtener_tipo_io(int socket_fd, t_log* logger);
bool io_sleep(t_pcb* pcb, t_io* io_sleep, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_sleep* peticion, t_logger* logger);
bool io_stdout(t_pcb* pcb, t_io* io_stdout, t_cola* block, t_cola_ready* ready,
               t_lista* susp_block, t_lista* susp_ready,
               t_peticion_stdout* peticion, t_logger* logger,
               t_socket_kernel_memory* socket_km);
bool io_stdin(t_pcb* pcb, t_io* io_stdin, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_stdin* peticion, t_logger* logger,
              t_socket_kernel_memory* socket_km);
bool retirar_lista_io(t_pcb* pcb, t_list* cola_io, t_logger* logger,
                      pthread_mutex_t* mutex_io);

#endif /* KERNEL_SCHEDULER_IO_H_ */

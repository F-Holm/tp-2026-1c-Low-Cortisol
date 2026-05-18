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
  pthread_mutex_t mutex_socket_io;
  pthread_mutex_t mutex_fin;
  t_pcb* proceso_actual;
  bool prioridad_activa;
  pthread_cond_t nuevo_proceso;
  t_cola_ready* cola_ready;
  t_lista* cola_block;
  t_lista* susp_block;
  t_lista* susp_ready;
  t_logger* logger;
  t_socket_kernel_memory* socket_km;
  int socket_server;
  bool cerrar_hilo;
  pthread_t hilo_io;
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
  pthread_mutex_t mutex_lista_stdin;
} t_lista_stdin;

typedef struct
{
  t_list* lista_stdout;
  pthread_mutex_t mutex_lista_stdout;
} t_lista_stdout;

typedef struct
{
  t_list* lista_sleep;
  pthread_mutex_t mutex_lista_sleep;
} t_lista_sleep;

typedef struct
{
  t_io* io;
  t_lista_stdin* lista_stdin;
} t_hilo_io_in;

typedef struct
{
  t_io* io;
  t_lista_stdout* lista_stdout;
} t_hilo_io_out;

typedef struct
{
  t_io* io;
  t_lista_sleep* lista_sleep;
} t_hilo_io_sleep;

typedef struct
{
  t_lista_stdin* lista_stdin;
  t_lista_stdout* lista_stdout;
  t_lista_sleep* lista_sleep;
} t_listas_io;

t_listas_io* inicializar_listas_io(void);

bool atender_nuevo_io(t_io io[3], int socket_fd, t_logger* logger,
                      t_socket_kernel_memory* socket_km, t_lista* block,
                      t_cola_ready* ready, t_lista* susp_block,
                      t_lista* susp_ready, t_listas_io* listas_io);

int obtener_tipo_io(int socket_fd, t_logger* logger);
bool procesar_nuevo_stdin(t_peticion_stdin* peticion, t_io* io_stdin,
                          t_lista_stdin* lista_stdin);
bool procesar_nuevo_stdout(t_peticion_stdout* peticion, t_io* io_stdout,
                           t_lista_stdout* lista_stdout, t_logger* logger);
bool procesar_nuevo_sleep(t_peticion_sleep* peticion, t_io* io_sleep,
                          t_lista_sleep* lista_sleep, t_logger* logger);
void cerrar_io(t_io io[3]);

#endif /* KERNEL_SCHEDULER_IO_H_ */

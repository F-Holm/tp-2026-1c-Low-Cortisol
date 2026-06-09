#ifndef KERNEL_SCHEDULER_IO_H_
#define KERNEL_SCHEDULER_IO_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/kernel_scheduler.h"
#include "utils/logger.h"

/****************** FUNCIONES DE IO ******************/

typedef struct
{
  t_list* lista_io;
  pthread_mutex_t mutex_lista_io;
} t_lista_io;
typedef struct
{
  int socket_io;
  pthread_mutex_t mutex_socket_io;
  pthread_mutex_t mutex_fin;
  t_pcb* proceso_actual;
  bool prioridad_activa;
  pthread_cond_t nuevo_proceso;
  t_colas* colas;
  t_logger* logger;
  t_socket_kernel_memory* socket_km;
  int socket_server;
  bool cerrar_hilo;
  pthread_t hilo_io;
  t_lista_io* lista_io;
  int tipo_io;
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

void inicializar_listas_io(t_io io[3]);

bool atender_nuevo_io(t_io io[3], int socket_fd, t_colas* colas,
                      bool prioridad_activa);

int obtener_tipo_io(int socket_fd, t_logger* logger);
bool procesar_nuevo_stdin(t_peticion_stdin* peticion, t_io* io_stdin,
                          t_pcb* pcb);
bool procesar_nuevo_stdout(t_peticion_stdout* peticion, t_io* io_stdout,
                           t_pcb* pcb);
bool procesar_nuevo_sleep(t_peticion_sleep* peticion, t_io* io_sleep,
                          t_pcb* pcb);
void cerrar_io(t_io io[3]);

#endif /* KERNEL_SCHEDULER_IO_H_ */

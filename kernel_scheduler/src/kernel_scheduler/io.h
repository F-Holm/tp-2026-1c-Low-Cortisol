#ifndef KERNEL_SCHEDULER_IO_H_
#define KERNEL_SCHEDULER_IO_H_

#include <pthread.h>
#include <stdbool.h>

#include "kernel_scheduler/kernel_scheduler.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/logger.h"
#include "utils/string.h"

/****************** FUNCIONES DE IO ******************/

typedef struct
{
  t_list* lista_io;
  pthread_mutex_t mutex_lista_io;
} t_lista_io;
typedef struct
{
  int socket_io;
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

t_io* crear_estructuras_io(void);
bool atender_nuevo_io(t_io io[3], int socket_fd, t_colas* colas,
                      bool prioridad_activa, int socket_server);
bool procesar_nuevo_io(void* peticion, t_io* io, t_pcb* pcb);
void cerrar_io(t_io* io);

#endif /* KERNEL_SCHEDULER_IO_H_ */

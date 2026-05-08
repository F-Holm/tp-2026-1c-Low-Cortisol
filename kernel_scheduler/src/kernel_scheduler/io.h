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
  int socket;
  pthread_mutex_t mutex_io;
  pthread_cond_t condicion_fin;
  t_pcb* proceso actual;
  t_list* cola_io;
  bool prioridad_activa;
  pthread_cond_t nuevo_proceso;
} t_io;

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger);
void cerrar_io(int sockets_io[3]);
int obtener_tipo_io(int socket_fd, t_log* logger);
void retirar_elem_cola_io(t_io* io, t_log* logger, t_pcb** pcb_post_io);
void reingresar_proceso(t_pcb** pcb_post_io,
                        t_kernel_scheduler_recursos* recursos);
bool io_stdin(t_io* io, t_cola_ready* cola_ready, t_peticion_stdin* peticion,
              t_kernel_scheduler_recursos* recursos);
#endif /* KERNEL_SCHEDULER_IO_H_ */

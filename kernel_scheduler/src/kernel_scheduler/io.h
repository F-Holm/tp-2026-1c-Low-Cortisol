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
  pthread_mutex_t mutex_io;
  pthread_cond_t condicion_fin;
  t_pcb* proceso_actual;
  t_list* cola_io;
  bool prioridad_activa;
  pthread_cond_t nuevo_proceso;
} t_io;

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger);
void cerrar_io(int sockets_io[3]);
int obtener_tipo_io(int socket_fd, t_log* logger);
bool io_sleep(t_pcb* pcb, t_io* io_sleep, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_sleep* peticion, t_logger* logger);
bool std_out(t_pcb* pcb, t_io* io_stdout, t_cola* block, t_cola_ready* ready,
             t_lista* susp_block, t_lista* susp_ready,
             t_peticion_stdout* peticion, t_logger* logger,
             t_socket_kernel_memory* socket_km);
bool io_stdin(t_pcb* pcb, t_io* io_stdin, t_cola* block, t_cola_ready* ready,
              t_lista* susp_block, t_lista* susp_ready,
              t_peticion_stdin* peticion, t_logger* logger,
              t_socket_kernel_memory* socket_km);
bool retirar_lista_io(t_pcb* pcb, t_list* cola_io, t_logger* logger);

#endif /* KERNEL_SCHEDULER_IO_H_ */

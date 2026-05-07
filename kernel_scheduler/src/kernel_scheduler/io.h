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

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger);
void cerrar_io(int sockets_io[3]);
int obtener_tipo_io(int socket_fd, t_log* logger);
void retirar_elem_cola(t_cola_mutex_io* cola_mutex, t_log* logger, t_proceso** pcb_post_io);
void reingresar_proceso(t_proceso** pcb_post_io, t_kernel_scheduler_recursos* recursos);
bool io_stdin(int sockets_io[], t_cola_mutex_io* cola_mutex, t_peticion_stdin* peticion, t_kernel_scheduler_recursos* recursos);
#endif /* KERNEL_SCHEDULER_IO_H_ */

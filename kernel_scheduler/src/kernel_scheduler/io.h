#ifndef KERNEL_SCHEDULER_IO_H_
#define KERNEL_SCHEDULER_IO_H_

#include <commons/collections/list.h>
#include <commons/collections/queue.h>
#include <commons/log.h>
#include <commons/string.h>
#include <pthread.h>
#include <stdbool.h>

struct
{
  pthread_mutex_t mutex_sockets_io;
  t_queue* cola_mutex;
} t_cola_mutex_io;

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger);
void cerrar_io(int sockets_io[3]);

#endif /* KERNEL_SCHEDULER_IO_H_ */

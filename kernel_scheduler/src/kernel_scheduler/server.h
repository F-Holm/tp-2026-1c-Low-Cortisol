#ifndef KERNEL_SCHEDULER_SERVER_H_
#define KERNEL_SCHEDULER_SERVER_H_

#include <commons/log.h>

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/mutex.h"
#include "kernel_scheduler/queue.h"

typedef struct
{
  int socket_server;
  t_logger* logger;
  t_lista_mutex* lista_mutex;
  t_colas* colas;
  t_socket_kernel_memory* socket_km;
} t_datos_servidor_escucha;

int crear_socket_servidor(char* puerto, t_log* logger);
t_datos_servidor_escucha* inicializar_datos_server_escucha(
    t_datos_servidor_escucha* datos, int socket_server, t_logger* logger,
    t_lista_mutex* lista_mutex, t_colas* colas,
    t_socket_kernel_memory* socket_kernel_memory);
void servidor_escucha(t_datos_servidor_escucha* datos);

#endif /* KERNEL_SCHEDULER_SERVER_H_ */

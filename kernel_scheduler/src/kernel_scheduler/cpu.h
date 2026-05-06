#ifndef KERNEL_SCHEDULER_CPU_H_
#define KERNEL_SCHEDULER_CPU_H_

#include <commons/collections/list.h>
#include <commons/log.h>
#include <pthread.h>

#include "kernel_scheduler/server.h"

typedef struct
{
  int socket_fd;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin;
  char* id;
} t_datos_hilo_cpu;

bool atender_nueva_cpu(t_datos_hilo_escucha* datos_hilo_escucha, int socket_cpu,
                       t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu);
void cerrar_cpu(t_list* lista_sockets_cpu,
                pthread_mutex_t* mutex_lista_sockets_cpu,
                pthread_cond_t* cond_fin_cpu);

#endif /* KERNEL_SCHEDULER_CPU_H_ */

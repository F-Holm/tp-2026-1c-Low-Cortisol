#ifndef KERNEL_SCHEDULER_CPU_H_
#define KERNEL_SCHEDULER_CPU_H_

#include <commons/collections/list.h>
#include <commons/log.h>
#include <pthread.h>
#include <stdint.h>

#include "kernel_scheduler/io.h"
#include "kernel_scheduler/mutex.h"
#include "kernel_scheduler/queue.h"

typedef struct
{
  int socket_fd;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin;
  char* id;
  t_logger* logger;
  t_lista_mutex* lista_mutex;
  t_colas* colas;
  t_io* estructuras_io;
  t_socket_kernel_memory* socket_km;
  int socket_servidor;
  pthread_mutex_t* mutex_desalojo;
} t_datos_hilo_cpu;

bool atender_nueva_cpu(int socket_cpu, t_list* lista_sockets_cpu,
                       pthread_mutex_t* mutex_lista_sockets_cpu,
                       pthread_cond_t* cond_fin_cpu, t_logger* logger,
                       t_lista_mutex* lista_mutex, t_colas* colas,
                       t_io* estructuras_io, t_socket_kernel_memory* socket_km,
                       int socket_servidor, pthread_mutex_t* mutex_desalojo);
void cerrar_cpu(t_list* lista_sockets_cpu,
                pthread_mutex_t* mutex_lista_sockets_cpu,
                pthread_cond_t* cond_fin_cpu);

#endif /* KERNEL_SCHEDULER_CPU_H_ */

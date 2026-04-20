#ifndef MEMORY_STICK_MEMORY_STICK_H_
#define MEMORY_STICK_MEMORY_STICK_H_

#include <commons/collections/list.h>
#include <commons/config.h>
#include <pthread.h>
#include <stdio.h>

#include "utils/client.h"
#include "utils/msg.h"

typedef struct
{
  pthread_t kernel_memory_hilo;
  pthread_mutex_t mutex_memory_sticks;
} t_hilo_cpu;

typedef struct
{
  char* id;

  int socket_kernel_memory;
  int socket_kernel_scheduler;

  t_list* memory_sticks;

  t_hilo_cpu hilos;

  t_log* logger;
  t_config* config;
} t_cpu;

void iniciar_hilo(void* arg);
void* escuchar_kernel_memory(void* arg);

#endif /* MEMORY_STICK_MEMORY_STICK_H_ */

#ifndef KERNEL_MEMORY_SV_H_
#define KERNEL_MEMORY_SV_H_

#include <commons/collections/list.h>
#include <commons/log.h>
typedef struct
{
  int socket_kernel_memory;
  t_log* logger;
  t_list* sticks_conectados;
  t_list* cpus_conectados;
} t_datos_kernel_mem;

typedef struct
{
  int socket_scheduler;
  t_log* logger;
} t_datos_scheduler;

typedef struct
{
  int id;
  int socket_cpu;
  t_log* logger;
} t_datos_cpu;

typedef struct
{
  int tamanio_stick;
  int socket_stick;
  char ip_memory_stick[16];
  int puerto_stick;
  t_log* logger;
} t_datos_stick;

typedef struct
{
  int socket_swap;
  t_log* logger;
} t_datos_swap;

#endif
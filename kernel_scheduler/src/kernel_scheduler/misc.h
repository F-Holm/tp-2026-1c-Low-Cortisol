#ifndef KERNEL_SCHEDULER_MISC_H_
#define KERNEL_SCHEDULER_MISC_H_

#include <commons/log.h>
#include <stdbool.h>
#include <commons/log.h>

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_algoritmo_planificacion;

typedef struct
{
  uint32_t pid;
  int prioridad;
  pthread_mutex_t mutex_pcb;
  int tiempo_suspendido;
} t_pcb;

typedef struct
{
  t_log* logger;
  pthread_mutex_t mutex_logger;
} t_logger;

typedef struct
{
  int socket_km;
  pthread_mutex_t mutex_socket;
} t_socket_kernel_memory;

bool responder_handshake(int socket_fd, int id_modulo, t_log* logger);

#endif /* KERNEL_SCHEDULER_MISC_H_ */
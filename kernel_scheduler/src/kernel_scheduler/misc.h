#ifndef KERNEL_SCHEDULER_MISC_H_
#define KERNEL_SCHEDULER_MISC_H_

#include <commons/log.h>
#include <stdbool.h>

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

bool responder_handshake(int socket_fd, int id_modulo, t_log* logger);

#endif /* KERNEL_SCHEDULER_MISC_H_ */
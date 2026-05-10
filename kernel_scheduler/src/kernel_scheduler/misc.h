#ifndef KERNEL_SCHEDULER_MISC_H_
#define KERNEL_SCHEDULER_MISC_H_

#include <commons/log.h>
#include <stdbool.h>
#include <stdint.h>

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
  unsigned long tiempo_bloqueado;
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

int get_prioridad_pcb(t_pcb* pcb);
t_pcb* crear_pcb(void);
void destruir_pcb(t_pcb* pcb);
bool responder_handshake(int socket_fd, int id_modulo, t_log* logger);
unsigned long millis(void);
unsigned long time_diff(unsigned long time_1, unsigned long time_2);

#endif /* KERNEL_SCHEDULER_MISC_H_ */

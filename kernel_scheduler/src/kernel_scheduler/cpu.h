#ifndef KERNEL_SCHEDULER_CPU_H_
#define KERNEL_SCHEDULER_CPU_H_

#include <commons/collections/list.h>
#include <commons/log.h>

typedef struct
{
  int socket_fd;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin_hilo_escucha;
} t_datos_hilo_cpu;

void* hilo_escucha_server(void* datos_hilo_escucha_void);
void* manejar_cliente_cpu(void* datos_hilo_cpu_void);

#endif /* KERNEL_SCHEDULER_CPU_H_ */

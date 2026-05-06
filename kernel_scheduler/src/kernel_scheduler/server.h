#ifndef KERNEL_SCHEDULER_SERVER_H_
#define KERNEL_SCHEDULER_SERVER_H_

#include <commons/log.h>

typedef struct
{
  int socket_server;
  t_log* logger;
} t_datos_hilo_escucha;

int crear_socket_servidor(char* puerto, t_log* logger);
t_datos_hilo_escucha* inicializar_datos_hilo_escucha(int socket_server,
                                                     t_log* logger);
void* hilo_escucha(void* datos_hilo_escucha_void);

#endif /* KERNEL_SCHEDULER_SERVER_H_ */

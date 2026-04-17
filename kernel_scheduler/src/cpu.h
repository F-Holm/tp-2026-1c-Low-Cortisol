#include <commons/collections/list.h>
#include <commons/log.h>
#include <stdatomic.h>

typedef struct
{
  int* socket_fd;
  t_list* lista_sockets;
  pthread_mutex_t* mutex_lista_sockets;
  pthread_cond_t* cond_fin_hilo_escucha;
} t_datos_hilo_cpu;
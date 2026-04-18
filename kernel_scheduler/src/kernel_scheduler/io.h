#include <commons/collections/list.h>
#include <commons/log.h>

typedef struct
{
  int* socket_fd;
  t_list* lista_sockets;
  // falta enum de tipo de io
} t_datos_hilo_io;

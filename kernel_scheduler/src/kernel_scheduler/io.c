#include "kernel_scheduler/io.h"

#include <sys/socket.h>

void cerrar_io(int* sockets_io)
{
  for (int i = 0; i < 3; i++)
    if (sockets_io[i] > 0)
      close(sockets_io[i]);
}

void agregar_io(int* sockets_io, int* socket_io, t_log* logger)
{
  int operacion = recibir_operacion(*socket_io);
  if (operacion != OP_TIPO_IO)
  {
    log_warning(logger, "## Código de operación no válido: %d", operacion);
    return;
  }

  char* tipo_io = recibir_string(*socket_io);
  if (strcmp(V_TIPO_IO[E_STDIN], tipo_io) == 0)
    sockets_io[E_STDIN] = *socket_io;
  else if (strcmp(V_TIPO_IO[E_STDOUT], tipo_io) == 0)
    sockets_io[E_STDOUT] = *socket_io;
  else if (strcmp(V_TIPO_IO[E_SLEEP], tipo_io) == 0)
    sockets_io[E_SLEEP] = *socket_io;
  else
  {
    log_warning(logger, "## IO de tipo %s Conectada", tipo_io);
    free(tipo_io);
    close(*socket_io);
    continue;
  }
  free(tipo_io);
  *socket_io = -1; // Así no cierra el socket
}

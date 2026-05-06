#include "kernel_scheduler/io.h"

#include <string.h>

#include "kernel_scheduler/misc.h"
#include "utils/io.h"
#include "utils/msg.h"

int obtener_tipo_io(int socket_fd, t_log* logger)
{
  if (recibir_operacion(socket_fd) != OP_TIPO_IO)
  {
    log_error(logger, "## Error en el tipo de operación. Expected: OP_TIPO_IO");
    return -1;
  }

  char* buffer = recibir_string(socket_fd);
  int tipo_io;

  if (strcmp(buffer, V_TIPO_IO[E_STDIN]))
    tipo_io = E_STDIN;
  else if (strcmp(buffer, V_TIPO_IO[E_STDOUT]))
    tipo_io = E_STDOUT;
  else if (strcmp(buffer, V_TIPO_IO[E_SLEEP]))
    tipo_io = E_SLEEP;
  else
  {
    log_error(logger, "## Tipo de IO no válido: %s", buffer);
    free(buffer);
    return -1;
  }

  log_info(logger, "## IO de tipo %s conectada", buffer);
  free(buffer);
  return tipo_io;
}

bool atender_nuevo_io(int sockets_io[3], int socket_fd, t_log* logger)
{
  if (!responder_handshake(socket_fd, MID_KERNEL_SCHEDULER, logger))
    return false;

  int tipo_io = obtener_tipo_io(socket_fd, logger);
  if (tipo_io == -1)
    return false;

  if (sockets_io[tipo_io] == -1)
  {
    log_error(logger, "## IO de tipo repetido, cerrando conexión");
    close(socket_fd);
    return false;
  }

  sockets_io[tipo_io] = socket_fd;
  return true;
}

void cerrar_io(int sockets_io[3])
{
  for (int i = 0; i < 3; i++)
  {
    if (sockets_io[i] > 0)
      close(sockets_io[i]);
  }
}

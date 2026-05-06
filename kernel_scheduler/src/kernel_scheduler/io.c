#include "kernel_scheduler/io.h"

bool atender_nuevo_io(int socket_io)
{
  if (!enviar_handshake(MID_MEMORY_STICK, socket_cpu))
  {
    log_error(logger, "## Error en el envio del Handshake con CPU");
    return false;
  }
  log_info(logger, "## Handshake exitoso con CPU");
  return true;
}

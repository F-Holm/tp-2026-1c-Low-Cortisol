#include "kernel_scheduler/misc.h"

#include "utils/msg.h"

bool responder_handshake(int socket_fd, int id_modulo, t_log* logger)
{
  if (!enviar_handshake(id_modulo, socket_fd))
  {
    log_error(logger, "## Error en el envio del Handshake con %s",
              HANDSHAKE_MSG[id_modulo]);
    return false;
  }
  log_info(logger, "## Handshake exitoso con %s", HANDSHAKE_MSG[id_modulo]);
  return true;
}

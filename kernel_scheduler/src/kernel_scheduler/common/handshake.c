#include "kernel_scheduler/common/handshake.h"

#include <stdbool.h>

#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

bool respond_handshake(t_socket* socket_fd, int id_module, t_log* logger)
{
  if (!send_handshake(id_module, socket_fd))
  {
    log_error(logger, "Error sending the handshake to %s",
              HANDSHAKE_MSG[id_module]);
    return false;
  }
  return true;
}

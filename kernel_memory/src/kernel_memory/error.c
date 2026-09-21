#include "error.h"

#include "kernel_memory/configurator.h"
#include "utils/log.h"
#include "utils/sockets.h"

void send_handshake_error(t_log* logger, t_socket* client_socket,
                          char* error_section)
{
  log_error(logger, "Error sending the handshake to %s", error_section);
  socket_destroy(client_socket);
}

void send_init_error(t_log* logger, t_socket* client_socket,
                     char* error_section)
{
  log_error(logger,
            "Closed the connection with %s because it could not "
            "be initialized correctly",
            error_section);
  close_communication(client_socket);
}

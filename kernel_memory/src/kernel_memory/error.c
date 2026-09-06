#include "error.h"

void send_handshake_error(t_log* logger, int client_socket, char* seccion_error)
{
  log_error(logger, "Error sending handshake for: %s", seccion_error);
  close(client_socket);
}

void send_init_error(t_log* logger, int client_socket, char* seccion_error)
{
  log_error(logger,
            "Closed the connection with %s because it could not "
            "be initialized correctly",
            seccion_error);
  close_communication(client_socket);
}
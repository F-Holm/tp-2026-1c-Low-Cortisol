#include "error.h"

void send_handshake_error(t_log* logger, int client_socket, char* error_section)
{
  log_error(logger, "Error sending handshake for: %s", error_section);
  close(client_socket);
}

void send_init_error(t_log* logger, int client_socket, char* error_section)
{
  log_error(logger,
            "Closed the connection with %s because it could not "
            "be initialized correctly",
            error_section);
  close_communication(client_socket);
}

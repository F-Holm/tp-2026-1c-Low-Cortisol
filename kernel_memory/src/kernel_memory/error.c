#include "error.h"

void enviar_handshake_error(t_log* logger, int client_socket,
                            char* seccion_error)
{
  log_error(logger, "Error al enviar handshake de: %s", seccion_error);
  close(client_socket);
}

void error_incorrecta_inicializacion(t_log* logger, int client_socket,
                                     char* seccion_error)
{
  log_error(logger,
            "Se ha terminado la conexion con %s ya que no se "
            "pudo inicializar correectametne",
            seccion_error);
  terminar_comunicacion(client_socket);
}
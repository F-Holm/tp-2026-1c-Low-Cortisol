#ifndef ERROR_H
#define ERROR_H
#include <unistd.h>

#include "configurador.h"
#include "estructuras.h"

void enviar_handshake_error(t_logger* logger, int client_socket,
                            char* seccion_error);
void error_incorrecta_inicializacion(t_logger* logger, int client_socket,
                                     char* seccion_error);

#endif  // ERROR_H
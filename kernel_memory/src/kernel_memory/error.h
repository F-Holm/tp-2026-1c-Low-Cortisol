#pragma once
#include <unistd.h>

#include "kernel_memory/configurador.h"
#include "kernel_memory/estructuras.h"

void enviar_handshake_error(t_log* logger, int client_socket,
                            char* seccion_error);
void error_incorrecta_inicializacion(t_log* logger, int client_socket,
                                     char* seccion_error);

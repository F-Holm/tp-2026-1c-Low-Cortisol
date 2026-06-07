#ifndef KERNEL_MEMORY_ERROR_H_
#define KERNEL_MEMORY_ERROR_H_
#include <unistd.h>

#include "kernel_memory/configurador.h"
#include "kernel_memory/estructuras.h"

void enviar_handshake_error(t_logger* logger, int client_socket,
                            char* seccion_error);
void error_incorrecta_inicializacion(t_logger* logger, int client_socket,
                                     char* seccion_error);

#endif  // KERNEL_MEMORY_ERROR_H_
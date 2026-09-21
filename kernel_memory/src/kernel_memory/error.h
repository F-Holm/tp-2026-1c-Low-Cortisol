#pragma once

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"

/** @brief Logs a handshake failure naming @p error_section and destroys the
 * socket, which nothing owns yet. */
void send_handshake_error(t_log* logger, t_socket* client_socket,
                          char* error_section);

/** @brief Logs an init failure naming @p error_section and closes the socket.
 */
void send_init_error(t_log* logger, t_socket* client_socket,
                     char* error_section);

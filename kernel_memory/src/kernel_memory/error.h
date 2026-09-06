#pragma once
#include <unistd.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"

void send_handshake_error(t_log* logger, int client_socket,
                          char* error_section);
void send_init_error(t_log* logger, int client_socket, char* error_section);

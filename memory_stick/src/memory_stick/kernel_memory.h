#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "utils/log.h"

bool handshake_km(int socket_km, t_log* logger);
bool send_size(int socket_km, char* size, t_log* logger);
int connect_to_kernel_memory(char* ip, char* port, char* size, t_log* logger);
int connect_km(char* ip, char* port, t_log* logger);
bool send_cpu_server_port_to_km(int socket, uint16_t port);

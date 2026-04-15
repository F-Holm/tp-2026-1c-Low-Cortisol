#ifndef MEMORY_STICK_KERNEL_MEMORY_H_
#define MEMORY_STICK_KERNEL_MEMORY_H_

#include <stdbool.h>
#include <stdint.h>

#include "utils/client.h"
#include "utils/server.h"

int conectar_km(char* ip, char* puerto);
bool handshake_km(int socket);
void enviar_puerto_server_ms_km(int socket, uint16_t puerto);

#endif /* MEMORY_STICK_KERNEL_MEMORY_H_ */

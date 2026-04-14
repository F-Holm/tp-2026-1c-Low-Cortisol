#ifndef MEMORY_STICK_KERNEL_MEMORY_H_
#define MEMORY_STICK_KERNEL_MEMORY_H_

#include "utils/client.h"
#include "utils/server.h"

int conectar_kernel_memory(char* ip, char* puerto);
void desconectar_kernel_memory(int socket);
bool handshake(int socket);

#endif /* MEMORY_STICK_KERNEL_MEMORY_H_ */

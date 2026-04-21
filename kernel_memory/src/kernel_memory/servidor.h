#ifndef KERNEL_MEMORY_SERVIDOR_H_
#define KERNEL_MEMORY_SERVIDOR_H_

#include "kernel_memory/estructuras.h"

void handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket);
void accept_cliente(void* ptr);

#endif
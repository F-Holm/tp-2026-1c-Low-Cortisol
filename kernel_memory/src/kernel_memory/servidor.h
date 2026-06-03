#ifndef KERNEL_MEMORY_SERVIDOR_H_
#define KERNEL_MEMORY_SERVIDOR_H_

#include "kernel_memory/configurador.h"
#include "kernel_memory/error.h"
#include "kernel_memory/escuchas.h"
#include "kernel_memory/estructuras.h"
#include "kernel_memory/inicializador.h"
#include "kernel_memory/protocolo.h"
#include "utils/msg.h"
#include "utils/server.h"

void handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket);
void accept_cliente(void* ptr);

#endif
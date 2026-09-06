#pragma once

#include "kernel_memory/configurador.h"
#include "kernel_memory/error.h"
#include "kernel_memory/escuchas.h"
#include "kernel_memory/estructuras.h"
#include "kernel_memory/inicializador.h"
#include "kernel_memory/protocolo.h"
#include "utils/msg.h"

bool handshake(t_datos_kernel_mem* datos_kernel_memory, int client_socket);
bool accept_cliente(void* ptr);

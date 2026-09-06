#pragma once

#include "kernel_memory/configurator.h"
#include "kernel_memory/error.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/listeners.h"
#include "kernel_memory/protocol.h"
#include "kernel_memory/structs.h"
#include "utils/msg.h"

bool handshake(t_kernel_memory_data* kernel_data, int client_socket);
bool accept_client(void* ptr);

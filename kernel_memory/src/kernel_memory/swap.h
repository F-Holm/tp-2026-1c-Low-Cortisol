#ifndef KERNEL_MEMORY_SWAP_H_
#define KERNEL_MEMORY_SWAP_H_

#include <commons/collections/list.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/estructuras.h"
#include "kernel_memory/protocolo.h"
t_list* filtrar_bloques_por_pid(t_list* bloques, uint32_t pid);

#endif // KERNEL_MEMORY_SWAP_H_
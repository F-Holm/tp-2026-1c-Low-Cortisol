#ifndef KERNEL_MEMORY_SWAP_H_
#define KERNEL_MEMORY_SWAP_H_

#include <commons/collections/list.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/estructuras.h"
#include "kernel_memory/protocolo.h"

void suspender_proceso(t_proceso* proceso_a_suspender, t_datos_scheduler* datos_scheduler);

#endif // KERNEL_MEMORY_SWAP_H_
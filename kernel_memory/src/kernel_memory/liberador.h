#ifndef KERNEL_MEMORY_LIBERADOR_H
#define KERNEL_MEMORY_LIBERADOR_H

#include <commons/collections/list.h>
#include <pthread.h>

#include "kernel_memory/estructuras.h"

void liberar_datos_kernel_mem(t_datos_kernel_mem* datos_kernel);
void liberar_datos_scheduler(t_datos_scheduler* datos_scheduler);
void liberar_datos_cpu(t_datos_cpu* datos_cpu);
void liberar_datos_stick(t_datos_stick* datos_stick);
void liberar_datos_swap(t_datos_swap* datos_swap);

#endif
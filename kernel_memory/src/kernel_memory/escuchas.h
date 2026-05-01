#ifndef KERNEL_MEMORY_ESCUCHAS_H_
#define KERNEL_MEMORY_ESCUCHAS_H_

#include "kernel_memory/estructuras.h"

void* escucha_scheduler(void* ptr);
void* escucha_cpu(void* ptr);
void* escucha_swap(void* ptr);
void* escucha_stick(void* ptr);
void empezar_escucha_scheduler(t_datos_scheduler* datos_scheduler);
void empezar_escucha_cpu(t_datos_cpu* datos_cpu);
void empezar_escucha_stick(t_datos_stick* datos_stick);
void empezar_escucha_swap(t_datos_swap* datos_swap);

#endif
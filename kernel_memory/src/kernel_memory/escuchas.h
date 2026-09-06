#pragma once

#include <pthread.h>
#include <stdlib.h>

#include "configurador.h"
#include "kernel_memory/estructuras.h"
#include "kernel_memory/inicializador.h"
#include "kernel_memory/liberador.h"
#include "kernel_memory/swap.h"
#include "protocolo.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/kernel_scheduler_cpu.h"
#include "utils/logger.h"
#include "utils/msg.h"

void* escucha_scheduler(void* ptr);
void* escucha_cpu(void* ptr);
void* escucha_swap(void* ptr);
void empezar_escucha_scheduler(t_datos_scheduler* datos_scheduler);
void empezar_escucha_cpu(t_datos_cpu* datos_cpu);
void empezar_escucha_swap(t_datos_swap* datos_swap);

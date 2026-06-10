#ifndef KERNEL_MEMORY_CONFIGURADOR_H_
#define KERNEL_MEMORY_CONFIGURADOR_H_

#include <commons/config.h>
#include <unistd.h>

#include "kernel_memory/estructuras.h"
#include "pthread.h"
#include "utils/logger.h"

t_logger* iniciar_logger(t_config* config);
t_config* iniciar_config(char* path);
void terminar_comunicacion(int socket_cliente);
char* iniciar_basepath(t_config* config);

int iniciar_instruction_delay(t_config* config);
int iniciar_compaction_delay(t_config* config);
int iniciar_segment_max_size(t_config* config);

#endif
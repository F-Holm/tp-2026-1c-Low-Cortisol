#ifndef KERNEL_MEMORY_CONFIGURADOR_H_
#define KERNEL_MEMORY_CONFIGURADOR_H_

#include <commons/config.h>
#include <commons/log.h>

t_log* iniciar_logger(t_config* config);
t_config* iniciar_config(char* path);
void terminar_comunicacion(int socket_cliente);

#endif
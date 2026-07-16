#ifndef KERNEL_SCHEDULER_KERNEL_MEMORY_H_
#define KERNEL_SCHEDULER_KERNEL_MEMORY_H_

#include <commons/log.h>

#include "kernel_scheduler/misc.h"
#include "utils/logger.h"

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_logger* logger);
bool avisar_terminar_proceso(t_socket_kernel_memory* socket_km, uint32_t pid,
                             int socket_servidor, t_logger* logger);

#endif /* KERNEL_SCHEDULER_KERNEL_MEMORY_H_ */

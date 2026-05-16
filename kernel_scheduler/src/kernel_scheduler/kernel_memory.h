#ifndef KERNEL_SCHEDULER_KERNEL_MEMORY_H_
#define KERNEL_SCHEDULER_KERNEL_MEMORY_H_

#include <commons/log.h>

#include "kernel_scheduler/misc.h"

int iniciar_conexion_kernel_memory(char* ip, char* puerto, t_log* logger);
bool avisar_nuevo_proceso(t_socket_kernel_memory* socket_km,
                          char* archivo_instrucciones, uint32_t pid);

#endif /* KERNEL_SCHEDULER_KERNEL_MEMORY_H_ */

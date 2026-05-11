#ifndef KERNEL_SCHEDULER_SHUTDOWN_H_
#define KERNEL_SCHEDULER_SHUTDOWN_H_

#include "kernel_scheduler/misc.h"

typedef enum
{
  MC_SIN_PROCESOS,
  MC_MEMORIA_CORRUPTA,
  MC_FALLO_CONEXION_KERNEL_MEMORY
} t_motivo_cierre;

void cerrar_kernel_scheduler(int socket_servidor, t_logger* logger,
                             int motivo_cierre);

#endif /* KERNEL_SCHEDULER_SHUTDOWN_H_ */

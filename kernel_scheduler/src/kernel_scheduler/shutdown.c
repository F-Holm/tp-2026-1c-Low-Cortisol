#include "kernel_scheduler/shutdown.h"

#include <pthread.h>
#include <stdbool.h>
#include <sys/socket.h>

static void log_shutdown(t_logger* logger, int motivo_cierre)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  switch (motivo_cierre)
  {
    case MC_SIN_PROCESOS:
      log_info(logger->logger, "## Procesos finalizados con éxito");
      break;
    case MC_MEMORIA_CORRUPTA:
      log_error(logger->logger, "## BSOD: Corrupción de memoria detectada");
      break;
    case MC_FALLO_CONEXION_KERNEL_MEMORY:
      log_error(logger->logger, "## Error en la conexión con kernel");
      break;
    default:
      log_error(logger->logger, "## Error desconocido");
      break;
  }
  pthread_mutex_unlock(&(logger->mutex_logger));
}

void cerrar_kernel_scheduler(int socket_servidor, t_logger* logger,
                             int motivo_cierre)
{
  static bool shutdown_activado = false;
  if (!shutdown_activado)
  {
    log_shutdown(logger, motivo_cierre);
    shutdown(socket_servidor, SHUT_RDWR);
    shutdown_activado = true;
  }
}

#include "kernel_scheduler/memory.h"

#include "utils/msg.h"

bool allocate_memory(t_syscall_memory* mem_alloc, t_logger* logger,
                     t_socket_kernel_memory* socket_km, int socket_server)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## <%d> - Solicitó syscall: <MEM_ALLOC>",
           mem_alloc->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  int mem_alloc_size = sizeof(t_syscall_memory);
  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool envio = enviar_buffer(OP_SYSCALL_MEM_ALLOC, mem_alloc, mem_alloc_size,
                             socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));

  if (!envio)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error en la comunicacion con el Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    cerrar_kernel_scheduler(socket_server, logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }

  // Ahora aguardo a que el km me envíe el "OK"
  pthread_mutex_lock(&(socket_km->mutex_socket));
  int cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    cerrar_kernel_scheduler(socket_server, logger, MC_MEMORIA_CORRUPTA);
    return false;
  }
  else
  {
    if (cod_op == OP_CODE_ERROR)
    {
      pthread_mutex_unlock(&(socket_km->mutex_socket));
      cerrar_kernel_scheduler(socket_server, logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
    }
  }
  char* respuesta = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  free(respuesta);
  return true;
}

bool free_memory(t_syscall_memory* mem_free, t_logger* logger,
                 t_socket_kernel_memory* socket_km, int socket_server)
{
  pthread_mutex_lock(&(logger->mutex_logger));
  log_info(logger->logger, "## <%d> - Solicitó syscall: <MEM_FREE>",
           mem_free->pid);
  pthread_mutex_unlock(&(logger->mutex_logger));

  int mem_free_size = sizeof(t_syscall_memory);
  pthread_mutex_lock(&(socket_km->mutex_socket));
  bool envio = enviar_buffer(OP_SYSCALL_MEM_FREE, mem_free, mem_free_size,
                             socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));

  if (!envio)
  {
    pthread_mutex_lock(&(logger->mutex_logger));
    log_error(logger->logger,
              "## Error en la comunicacion con el Kernel Memory");
    pthread_mutex_unlock(&(logger->mutex_logger));
    cerrar_kernel_scheduler(socket_server, logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return false;
  }

  // Ahora aguardo a que el km me envíe el "OK"
  pthread_mutex_lock(&(socket_km->mutex_socket));
  int cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    cerrar_kernel_scheduler(socket_server, logger, MC_MEMORIA_CORRUPTA);
    return false;
  }
  else
  {
    if (cod_op == OP_CODE_ERROR)
    {
      pthread_mutex_unlock(&(socket_km->mutex_socket));
      cerrar_kernel_scheduler(socket_server, logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return false;
    }
  }
  char* respuesta = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  free(respuesta);
  return true;
}

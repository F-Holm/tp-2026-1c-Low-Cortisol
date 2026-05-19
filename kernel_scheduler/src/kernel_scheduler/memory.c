#include "kernel_scheduler/memory.h"

#include "utils/msg.h"

bool allocate_memory(t_syscall_memory* mem_alloc, t_logger* logger,
                     t_socket_kernel_memory* socket_km)
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
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error en la comunicacion con el Kernel Memory");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_CONEXION_KM;
  }

  // Ahora aguardo a que el km me envíe el "OK"
  pthread_mutex_lock(&(socket_km->mutex_socket));
  int cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    cerrar_kernel_scheduler(hilo_stdin->io->socket_server,
                            hilo_stdin->io->logger,
                            MC_FALLO_CONEXION_KERNEL_MEMORY);
    return D_ERROR_KM;
  }
  else
  {
    if (cod_op == OP_CODE_ERROR)
    {
      pthread_mutex_unlock(&(socket_km->mutex_socket));
      cerrar_kernel_scheduler(hilo_stdin->io->socket_server,
                              hilo_stdin->io->logger,
                              MC_FALLO_CONEXION_KERNEL_MEMORY);
      return D_ERROR_CONEXION_KM;
    }
  }
  char* = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  return D_TODO_BIEN;
}

bool free_memory(t_syscall_memory* mem_free, t_logger* logger,
                 t_socket_kernel_memory* socket_km)
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
    pthread_mutex_lock(&(hilo_out->io->logger->mutex_logger));
    log_error(hilo_out->io->logger->logger,
              "## Error en la comunicacion con el Kernel Memory");
    pthread_mutex_unlock(&(hilo_out->io->logger->mutex_logger));
    return D_ERROR_CONEXION_KM;
  }

  // Ahora aguardo a que el km me envíe el "OK"
  pthread_mutex_lock(&(socket_km->mutex_socket));
  int cod_op = recibir_operacion(socket_km->socket_km);
  if (cod_op == OP_MEMORIA_CORRUPTA)
  {
    pthread_mutex_unlock(&(socket_km->mutex_socket));
    return D_ERROR_KM;
  }
  else
  {
    if (cod_op == OP_CODE_ERROR)
    {
      pthread_mutex_unlock(&(socket_km->mutex_socket));
      return D_ERROR_CONEXION_KM;
    }
  }
  char* = recibir_string(socket_km->socket_km);
  pthread_mutex_unlock(&(socket_km->mutex_socket));
  return D_TODO_BIEN;
}

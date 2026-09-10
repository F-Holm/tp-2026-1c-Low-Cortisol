#include "kernel_scheduler/syscalls/memory.h"
#include "kernel_scheduler/shutdown.h"

#include "utils/msg.h"

static bool response_km_mem_alloc(t_queues* queues);
static bool has_space(t_syscall_memory* mem_alloc, t_queues* queues);
static bool response_km_mem_free(t_queues* queues);

bool allocate_memory(t_syscall_memory* mem_alloc, t_queues* queues)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  if (!has_space(mem_alloc, queues))
  {
    log_debug(queues->logger, "Not enough space");
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  int mem_alloc_size = sizeof(t_syscall_memory);
  bool comms = send_buffer(OP_CREATE_SEGMENT, mem_alloc, mem_alloc_size,
                           queues->km_socket->km_socket);

  if (!comms)
  {
    log_warning(queues->logger, "Error communicating with Kernel Memory");
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  comms = response_km_mem_alloc(queues);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return comms;
}

bool free_memory(t_syscall_memory* mem_free, t_queues* queues)
{
  int mem_free_size = sizeof(t_syscall_memory);
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  bool comms = send_buffer(OP_DELETE_SEGMENT, mem_free, mem_free_size,
                           queues->km_socket->km_socket);

  if (!comms)
  {
    log_warning(queues->logger, "Error communicating with Kernel Memory");
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
    return false;
  }

  // Now wait for the km to send the "OK"
  comms = response_km_mem_free(queues);
  create_resumption_routine_thread(queues);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return comms;
}

static bool response_km_mem_alloc(t_queues* queues)
{
  int cod_op = -1;

  cod_op = receive_op_code(queues->km_socket->km_socket);

  switch (cod_op)
  {
    case OP_MEMORY_CORRUPTED:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return true;
    case OP_SEGMENT_SIZE_EXCEEDED:
      free(receive_string(queues->km_socket->km_socket));
      return false;
    case OP_MEMORY_ALLOCATED:
      free(receive_string(queues->km_socket->km_socket));
      return true;
    case OP_COMPACTION_NEEDED:
      free(receive_string(queues->km_socket->km_socket));
      routine_compaction(queues);
      return response_km_mem_alloc(queues);
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return response_km_mem_alloc(queues);
    default:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return true;
  }
}

static bool has_space(t_syscall_memory* mem_alloc, t_queues* queues)
{
  int space = space_available_no_mutex(queues, mem_alloc->pid);
  return space >= mem_alloc->size;
}

static bool response_km_mem_free(t_queues* queues)
{
  int cod_op = -1;

  cod_op = receive_op_code(queues->km_socket->km_socket);

  switch (cod_op)
  {
    case OP_MEMORY_CORRUPTED:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      return false;
    case OP_MEMORY_FREED:
      free(receive_string(queues->km_socket->km_socket));
      return true;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return response_km_mem_free(queues);
    default:
      free(receive_string(queues->km_socket->km_socket));
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      return false;
  }
}

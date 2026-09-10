#include "kernel_scheduler/scheduler/memory_query.h"

#include <stdlib.h>

#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/msg.h"

static int receive_space(t_queues* queues);
static int receive_size(t_queues* queues);
static int receive_size_no_logger(t_queues* queues);
static int process_size_no_mutex_no_logger(t_queues* queues, uint32_t pid);

int space_available_no_mutex(t_queues* queues, uint32_t pid)
{
  if (!(send_string(OP_REQUEST_FREE_MEMORY, "Requesting the available space",
                    queues->km_socket->km_socket)))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    return -1;
  }
  return receive_space(queues);
}

int space_available(t_queues* queues, uint32_t pid)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  int space = space_available_no_mutex(queues, pid);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return space;
}

int process_size_no_mutex(t_queues* queues, uint32_t pid)
{
  if (!(send_buffer(OP_REQUEST_PROCESS_SIZE, &pid, sizeof(uint32_t),
                    queues->km_socket->km_socket)))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    return -1;
  }

  return receive_size(queues);
}

int process_size(t_queues* queues, uint32_t pid)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  int space = process_size_no_mutex(queues, pid);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return space;
}

int process_size_no_logger(t_queues* queues, uint32_t pid)
{
  pthread_mutex_lock(&(queues->km_socket->socket_mutex));
  int space = process_size_no_mutex_no_logger(queues, pid);
  pthread_mutex_unlock(&(queues->km_socket->socket_mutex));
  return space;
}

static int receive_space(t_queues* queues)
{
  int op_code = receive_op_code(queues->km_socket->km_socket);

  switch (op_code)
  {
    case OP_FREE_MEMORY:
      int space;
      int* aux = receive_buffer(&space, queues->km_socket->km_socket);
      space = *aux;
      free(aux);
      log_trace(queues->logger, "Space available: %d", space);
      return space;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return receive_space(queues);
      break;
    case OP_MEMORY_CORRUPTED:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      break;
    default:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      break;
  }
  return -1;
}

static int receive_size(t_queues* queues)
{
  int op_code = receive_op_code(queues->km_socket->km_socket);

  switch (op_code)
  {
    case OP_PROCESS_SIZE:
      int space;
      int* aux = receive_buffer(&space, queues->km_socket->km_socket);
      space = *aux;
      free(aux);
      log_trace(queues->logger, "Process size: %d", space);
      return space;
      break;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return receive_size(queues);
      break;
    case OP_MEMORY_CORRUPTED:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      break;
    default:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      break;
  }
  return -1;
}

static int receive_size_no_logger(t_queues* queues)
{
  int op_code = receive_op_code(queues->km_socket->km_socket);

  switch (op_code)
  {
    case OP_PROCESS_SIZE:
      int space;
      int* aux = receive_buffer(&space, queues->km_socket->km_socket);
      space = *aux;
      free(aux);
      return space;
      break;
    case OP_NEW_MEMORY_STICK:
      free(receive_string(queues->km_socket->km_socket));
      create_resumption_routine_thread(queues);
      return receive_size(queues);
      break;
    case OP_MEMORY_CORRUPTED:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_CORRUPTED_MEMORY, -1);
      break;
    default:
      close_kernel_scheduler(queues->server_socket, queues->logger,
                             SR_KERNEL_MEMORY_CONNECTION_FAILURE, -1);
      break;
  }
  return -1;
}

static int process_size_no_mutex_no_logger(t_queues* queues, uint32_t pid)
{
  if (!(send_buffer(OP_REQUEST_PROCESS_SIZE, &pid, sizeof(uint32_t),
                    queues->km_socket->km_socket)))
  {
    close_kernel_scheduler(queues->server_socket, queues->logger,
                           SR_KERNEL_MEMORY_SEND_ERROR,
                           queues->km_socket->km_socket);
    return -1;
  }

  return receive_size_no_logger(queues);
}

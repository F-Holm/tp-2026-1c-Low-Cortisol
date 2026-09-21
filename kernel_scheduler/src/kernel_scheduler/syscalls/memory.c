#include "kernel_scheduler/syscalls/memory.h"

#include <stdbool.h>
#include <stdlib.h>

#include "kernel_scheduler/scheduler/compaction.h"
#include "kernel_scheduler/scheduler/kernel_memory_reply.h"
#include "kernel_scheduler/scheduler/memory_query.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/syscalls.h"

static bool response_km_mem_alloc(t_queues* queues);
static bool has_space(t_syscall_memory* mem_alloc, t_queues* queues);
static bool response_km_mem_free(t_queues* queues);

bool allocate_memory(t_syscall_memory* mem_alloc, t_queues* queues)
{
  socket_mutex_lock(queues->km_socket);
  if (!has_space(mem_alloc, queues))
  {
    log_debug(queues->logger, "Not enough space");
    socket_mutex_unlock(queues->km_socket);
    return false;
  }

  int mem_alloc_size = sizeof(t_syscall_memory);
  bool comms = send_buffer(OP_CREATE_SEGMENT, mem_alloc, mem_alloc_size,
                           queues->km_socket);

  if (!comms)
  {
    log_warning(queues->logger, "Error communicating with Kernel Memory");
    close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);
    socket_mutex_unlock(queues->km_socket);
    return false;
  }

  comms = response_km_mem_alloc(queues);
  socket_mutex_unlock(queues->km_socket);
  return comms;
}

bool free_memory(t_syscall_memory* mem_free, t_queues* queues)
{
  int mem_free_size = sizeof(t_syscall_memory);
  socket_mutex_lock(queues->km_socket);
  bool comms = send_buffer(OP_DELETE_SEGMENT, mem_free, mem_free_size,
                           queues->km_socket);

  if (!comms)
  {
    log_warning(queues->logger, "Error communicating with Kernel Memory");
    close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);
    socket_mutex_unlock(queues->km_socket);
    return false;
  }

  // Now wait for the km to send the "OK"
  comms = response_km_mem_free(queues);
  create_resumption_routine_thread(queues);
  socket_mutex_unlock(queues->km_socket);
  return comms;
}

static bool response_km_mem_alloc(t_queues* queues)
{
  int op_code =
      RECEIVE_KM_OPCODE(queues, OP_MEMORY_ALLOCATED, OP_SEGMENT_SIZE_EXCEEDED,
                        OP_NOT_ENOUGH_MEMORY, OP_COMPACTION_NEEDED);

  switch (op_code)
  {
    case OP_MEMORY_ALLOCATED:
      free(receive_string(queues->km_socket));
      return true;
    case OP_SEGMENT_SIZE_EXCEEDED:
      free(receive_string(queues->km_socket));
      return false;
    case OP_NOT_ENOUGH_MEMORY:
      free(receive_string(queues->km_socket));
      log_debug(queues->logger, "Not enough space");
      return false;
    case OP_COMPACTION_NEEDED:
      free(receive_string(queues->km_socket));
      routine_compaction(queues);
      return response_km_mem_alloc(queues);
    default:
      // The scheduler is already shutting down: do not fail the allocation too.
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
  if (RECEIVE_KM_OPCODE(queues, OP_MEMORY_FREED) == OP_CODE_ERROR)
    return false;

  free(receive_string(queues->km_socket));
  return true;
}

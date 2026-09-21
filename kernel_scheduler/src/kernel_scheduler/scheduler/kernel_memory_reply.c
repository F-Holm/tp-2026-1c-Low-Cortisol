#include "kernel_scheduler/scheduler/kernel_memory_reply.h"

#include <stdbool.h>
#include <stdlib.h>

#include "kernel_scheduler/scheduler/compaction.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/shutdown.h"
#include "utils/msg.h"

static bool is_expected(int op_code, const int* expected, int count);

int receive_km_opcode(t_queues* queues, const int* expected, int count)
{
  while (true)
  {
    int op_code = receive_op_code(queues->km_socket);
    if (is_expected(op_code, expected, count))
    {
      return op_code;
    }

    switch (op_code)
    {
      case OP_NEW_MEMORY_STICK:
        free(receive_string(queues->km_socket));
        create_resumption_routine_thread(queues);
        break;
      case OP_MEMORY_CORRUPTED:
        free(receive_string(queues->km_socket));
        close_kernel_scheduler(SR_CORRUPTED_MEMORY);
        return OP_CODE_ERROR;
      default:
        free(receive_string(queues->km_socket));
        close_kernel_scheduler(SR_KERNEL_MEMORY_CONNECTION_FAILURE);
        return OP_CODE_ERROR;
    }
  }
}

static bool is_expected(int op_code, const int* expected, int count)
{
  for (int i = 0; i < count; i++)
  {
    if (op_code == expected[i])
    {
      return true;
    }
  }
  return false;
}

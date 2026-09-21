#include "kernel_scheduler/shutdown.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "utils/msg.h"

const char* const SHUTDOWN_REASONS[4] = {
    "Processes finished successfully", "BSOD: Corruption of memory detected",
    "Connection error with Kernel Memory", "Unknown error"};

static struct
{
  t_socket* server_socket;
  t_log* logger;
  t_socket* km_socket;
} shutdown_ctx;

static void log_shutdown(int reason_shutdown);
static void check_reason_shutdown(int* reason_shutdown);
static void notify_shutdown_kernel_memory(int reason_shutdown);

void init_shutdown(t_socket* server_socket, t_log* logger, t_socket* km_socket)
{
  shutdown_ctx.server_socket = server_socket;
  shutdown_ctx.logger = logger;
  shutdown_ctx.km_socket = km_socket;
}

void close_kernel_scheduler(int reason_shutdown)
{
  static atomic_bool shutdown_activado = false;
  if (!atomic_exchange(&shutdown_activado, true))
  {
    notify_shutdown_kernel_memory(reason_shutdown);
    check_reason_shutdown(&reason_shutdown);
    log_shutdown(reason_shutdown);
    socket_shutdown(shutdown_ctx.server_socket, SOCKET_SHUTDOWN_BOTH);
  }
}

static void log_shutdown(int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_info(shutdown_ctx.logger, "Shutting down: %s",
             SHUTDOWN_REASONS[reason_shutdown]);
  }
  else
  {
    log_error(shutdown_ctx.logger, "Shutting down: %s",
              SHUTDOWN_REASONS[reason_shutdown]);
  }
}

static void check_reason_shutdown(int* reason_shutdown)
{
  if (*reason_shutdown != SR_KERNEL_MEMORY_SEND_ERROR)
  {
    return;
  }

  bool keep_running = true;
  while (keep_running)
  {
    switch (receive_op_code(shutdown_ctx.km_socket))
    {
      case OP_CODE_ERROR:
        *reason_shutdown = SR_KERNEL_MEMORY_CONNECTION_FAILURE;
        keep_running = false;
        break;
      case OP_MEMORY_CORRUPTED:
        *reason_shutdown = SR_CORRUPTED_MEMORY;
        keep_running = false;
        break;
      default:
        free(receive_string(shutdown_ctx.km_socket));
        break;
    }
  }
}

static void notify_shutdown_kernel_memory(int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_debug(shutdown_ctx.logger,
              "Notifying Kernel Memory of the Kernel Scheduler shutdown");
    send_string(OP_KERNEL_SCHEDULER_SHUTDOWN, "No more processes to run",
                shutdown_ctx.km_socket);
  }
}

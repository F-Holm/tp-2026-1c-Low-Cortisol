#include "kernel_scheduler/shutdown.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "utils/msg.h"

const char* const SHUTDOWN_REASONS[4] = {
    "Processes finished successfully", "BSOD: Corruption of memory detected",
    "Connection error with Kernel Memory", "Unknown error"};

static void log_shutdown(t_log* logger, int reason_shutdown);
static void check_reason_shutdown(int* reason_shutdown, int km_socket);
static void notify_shutdown_kernel_memory(int reason_shutdown, int km_socket,
                                          t_log* logger);

void close_kernel_scheduler(int server_socket, t_log* logger,
                            int reason_shutdown, int km_socket)
{
  static atomic_bool shutdown_activado = false;
  if (!atomic_exchange(&shutdown_activado, true))
  {
    notify_shutdown_kernel_memory(reason_shutdown, km_socket, logger);
    check_reason_shutdown(&reason_shutdown, km_socket);
    log_shutdown(logger, reason_shutdown);
    shutdown(server_socket, SHUT_RDWR);
  }
}

static void log_shutdown(t_log* logger, int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_info(logger, "Shutting down: %s", SHUTDOWN_REASONS[reason_shutdown]);
  }
  else
  {
    log_error(logger, "Shutting down: %s", SHUTDOWN_REASONS[reason_shutdown]);
  }
}

static void check_reason_shutdown(int* reason_shutdown, int km_socket)
{
  if (*reason_shutdown != SR_KERNEL_MEMORY_SEND_ERROR)
  {
    return;
  }

  bool keep_running = true;
  while (keep_running)
  {
    switch (receive_op_code(km_socket))
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
        free(receive_string(km_socket));
        break;
    }
  }
}

static void notify_shutdown_kernel_memory(int reason_shutdown, int km_socket,
                                          t_log* logger)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_debug(logger,
              "Notifying Kernel Memory of the Kernel Scheduler shutdown");
    send_string(OP_KERNEL_SCHEDULER_SHUTDOWN, "No more processes to run",
                km_socket);
  }
}

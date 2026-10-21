#include "kernel_scheduler/shutdown.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

const char* const SHUTDOWN_REASONS[4] = {
    "Processes finished successfully", "BSOD: Corruption of memory detected",
    "Connection error with Kernel Memory", "Unknown error"};

static void log_shutdown(t_shutdown_ctx* ctx, int reason_shutdown);
static void check_reason_shutdown(t_shutdown_ctx* ctx, int* reason_shutdown);
static void notify_shutdown_kernel_memory(t_shutdown_ctx* ctx,
                                          int reason_shutdown);

t_shutdown_ctx* init_shutdown(t_socket* server_socket, t_log* logger,
                              t_socket* km_socket)
{
  t_shutdown_ctx* ctx = malloc(sizeof(t_shutdown_ctx));
  ctx->server_socket = server_socket;
  ctx->logger = logger;
  ctx->km_socket = km_socket;
  return ctx;
}

void destroy_shutdown(t_shutdown_ctx* ctx)
{
  free(ctx);
}

void close_kernel_scheduler(t_shutdown_ctx* ctx, int reason_shutdown)
{
  static atomic_bool shutdown_activado = false;
  if (!atomic_exchange(&shutdown_activado, true))
  {
    notify_shutdown_kernel_memory(ctx, reason_shutdown);
    check_reason_shutdown(ctx, &reason_shutdown);
    log_shutdown(ctx, reason_shutdown);
    socket_shutdown(ctx->server_socket, SOCKET_SHUTDOWN_BOTH);
  }
}

static void log_shutdown(t_shutdown_ctx* ctx, int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_info(ctx->logger, "Shutting down: %s",
             SHUTDOWN_REASONS[reason_shutdown]);
  }
  else
  {
    log_error(ctx->logger, "Shutting down: %s",
              SHUTDOWN_REASONS[reason_shutdown]);
  }
}

static void check_reason_shutdown(t_shutdown_ctx* ctx, int* reason_shutdown)
{
  if (*reason_shutdown != SR_KERNEL_MEMORY_SEND_ERROR)
  {
    return;
  }

  bool keep_running = true;
  while (keep_running)
  {
    switch (receive_op_code(ctx->km_socket))
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
        free(receive_string(ctx->km_socket));
        break;
    }
  }
}

static void notify_shutdown_kernel_memory(t_shutdown_ctx* ctx,
                                          int reason_shutdown)
{
  if (reason_shutdown == SR_NO_PROCESSES)
  {
    log_debug(ctx->logger,
              "Notifying Kernel Memory of the Kernel Scheduler shutdown");
    send_string(OP_KERNEL_SCHEDULER_SHUTDOWN, "No more processes to run",
                ctx->km_socket);
  }
}

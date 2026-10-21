#include "kernel_memory/stick_watchdog.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "kernel_memory/structs.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/watchdog.h"

typedef struct
{
  t_kernel_memory_data* kernel_data;
  bool already_notified;
} t_stick_watchdog_ctx;

static t_socket* stick_socket(void* element);
static void on_stick_disconnected(void* arg);

t_watchdog* start_stick_watchdog(t_kernel_memory_data* kernel_data)
{
  t_stick_watchdog_ctx* ctx = malloc(sizeof(t_stick_watchdog_ctx));
  ctx->kernel_data = kernel_data;
  ctx->already_notified = false;

  t_watchdog* watchdog = watchdog_start(
      kernel_data->connected_sticks, stick_socket,
      kernel_data->socket_list_mutex, 500, on_stick_disconnected, ctx);
  if (watchdog == NULL)
  {
    log_error(kernel_data->logger,
              "Error creating the memory stick connection-check thread");
    free(ctx);
  }
  return watchdog;
}

void destroy_stick_watchdog(t_watchdog* watchdog)
{
  t_stick_watchdog_ctx* ctx = watchdog->callback_arg;
  watchdog_destroy(watchdog);
  free(ctx);
}

// connected_sticks holds t_stick_data*, not t_socket* directly -- this is
// the extractor utils/watchdog.h calls to get the socket to ping out of
// each element.
static t_socket* stick_socket(void* element)
{
  return ((t_stick_data*)element)->socket_stick;
}

// Pings every connected stick over the same socket/mutex the rest of Kernel
// Memory uses to talk to it, so this never races a real read/write. A ping
// is a lightweight OP_CONNECTION_CHECK send with no reply expected -- the
// stick just needs to tolerate the opcode instead of closing on it (see
// utils/watchdog.c).
static void on_stick_disconnected(void* arg)
{
  t_stick_watchdog_ctx* ctx = arg;
  t_socket* socket_scheduler =
      atomic_load(&(ctx->kernel_data->socket_scheduler));
  if (ctx->already_notified || socket_scheduler == NULL)
  {
    return;
  }
  ctx->already_notified = true;

  log_warning(ctx->kernel_data->logger,
              "Notifying the Kernel Scheduler that memory is corrupted");
  if (!send_string(OP_MEMORY_CORRUPTED, "Stick not available",
                   socket_scheduler))
  {
    log_error(ctx->kernel_data->logger,
              "Could not send the BSOD to the Kernel Scheduler");
  }
}

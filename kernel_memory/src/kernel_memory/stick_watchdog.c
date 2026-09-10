#include "kernel_memory/stick_watchdog.h"

#include <stdlib.h>
#include <unistd.h>

#include "utils/collections/list.h"
#include "utils/msg.h"

static void* watch_sticks(void* args);
static bool any_stick_unreachable(t_kernel_memory_data* kernel_data);
static void notify_scheduler_memory_corrupted(t_kernel_memory_data* kernel_data,
                                              t_stick_watchdog* watchdog);

t_stick_watchdog* start_stick_watchdog(t_kernel_memory_data* kernel_data)
{
  t_stick_watchdog* watchdog = malloc(sizeof(t_stick_watchdog));
  watchdog->kernel_data = kernel_data;
  atomic_init(&(watchdog->close), false);
  watchdog->already_notified = false;

  if (pthread_create(&(watchdog->thread), NULL, watch_sticks, watchdog) != 0)
  {
    log_error(kernel_data->logger,
             "Error creating the memory stick connection-check thread");
  }
  return watchdog;
}

void destroy_stick_watchdog(t_stick_watchdog* watchdog)
{
  atomic_store(&(watchdog->close), true);
  pthread_join(watchdog->thread, NULL);
  free(watchdog);
}

static void* watch_sticks(void* args)
{
  t_stick_watchdog* watchdog = (t_stick_watchdog*)args;
  bool keep_running = true;

  while (keep_running)
  {
    usleep(500000);

    if (any_stick_unreachable(watchdog->kernel_data))
    {
      notify_scheduler_memory_corrupted(watchdog->kernel_data, watchdog);
    }

    keep_running = !atomic_load(&(watchdog->close));
  }
  return NULL;
}

// Pings every connected stick over the same socket/mutex the rest of Kernel
// Memory uses to talk to it, so this never races a real read/write. A ping
// is a lightweight OP_KERNEL_MEMORY_RUNNING send with no reply expected --
// the stick just needs to tolerate the opcode instead of closing on it.
static bool any_stick_unreachable(t_kernel_memory_data* kernel_data)
{
  bool unreachable = false;
  pthread_mutex_lock(kernel_data->socket_list_mutex);
  int stick_count = list_size(kernel_data->connected_sticks);
  for (int i = 0; i < stick_count && !unreachable; i++)
  {
    t_stick_data* stick = list_get(kernel_data->connected_sticks, i);
    if (!send_string(OP_KERNEL_MEMORY_RUNNING, "Is the stick still connected?",
                     stick->socket_stick))
    {
      unreachable = true;
    }
  }
  pthread_mutex_unlock(kernel_data->socket_list_mutex);
  return unreachable;
}

static void notify_scheduler_memory_corrupted(t_kernel_memory_data* kernel_data,
                                              t_stick_watchdog* watchdog)
{
  if (watchdog->already_notified || kernel_data->socket_scheduler == -1)
    return;
  watchdog->already_notified = true;

  log_warning(kernel_data->logger,
             "Notifying the Kernel Scheduler that memory is corrupted");
  if (!send_string(OP_MEMORY_CORRUPTED, "Stick not available",
                   kernel_data->socket_scheduler))
  {
    log_error(kernel_data->logger,
             "Could not send the BSOD to the Kernel Scheduler");
  }
}

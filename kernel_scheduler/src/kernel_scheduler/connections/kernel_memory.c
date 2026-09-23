#include "kernel_scheduler/connections/kernel_memory.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <threads.h>

#include "kernel_scheduler/shutdown.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/time.h"

static t_socket* connect_kernel_memory(char* ip, char* port, t_log* logger);
static bool handshake_kernel_memory(t_socket* km_socket, t_log* logger);
static int thread_check_connection_kernel_memory(void* args);

t_socket* start_connection_kernel_memory(char* ip, char* port, t_log* logger)
{
  t_socket* km_socket = connect_kernel_memory(ip, port, logger);
  if (km_socket == NULL)
    return NULL;

  if (!handshake_kernel_memory(km_socket, logger))
  {
    socket_destroy(km_socket);
    return NULL;
  }

  return km_socket;
}

bool notify_terminate_process(t_socket* km_socket, uint32_t pid)
{
  socket_mutex_lock(km_socket);
  bool ret = send_buffer(OP_END_PROCESS, &pid, sizeof(uint32_t), km_socket);
  if (!ret)
  {
    close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);
  }
  socket_mutex_unlock(km_socket);
  return ret;
}

t_connection_check_thread* start_thread_check_connection_kernel_memory(
    t_log* logger, t_socket* km_socket)
{
  t_connection_check_thread* data = malloc(sizeof(t_connection_check_thread));
  data->logger = logger;
  data->km_socket = km_socket;
  atomic_init(&(data->close), false);

  if (thrd_create(&(data->thread), thread_check_connection_kernel_memory,
                  data) != 0)
  {
    log_error(logger,
              "Error creating the connection-check thread for the "
              "Kernel Memory connection");
  }
  return data;
}

void destroy_thread_check_connection_kernel_memory(
    t_connection_check_thread* data)
{
  atomic_store(&(data->close), true);
  thrd_join(data->thread, NULL);
  free(data);
}

static t_socket* connect_kernel_memory(char* ip, char* port, t_log* logger)
{
  t_socket* km_socket = socket_create(SOCKET_KIND_CLIENT, ip, port, true);
  if (km_socket == NULL)
  {
    log_error(logger, "Connection error with Kernel Memory");
    return NULL;
  }
  log_info(logger, "Connected to Kernel Memory");
  return km_socket;
}

static bool handshake_kernel_memory(t_socket* km_socket, t_log* logger)
{
  if (!send_handshake(MID_KERNEL_SCHEDULER, km_socket))
  {
    log_error(logger, "Error sending the handshake to Kernel Memory");
    return false;
  }
  if (receive_handshake(km_socket) != MID_KERNEL_MEMORY)
  {
    log_error(logger, "Error receiving the handshake from Kernel Memory");
    return false;
  }
  log_debug(logger, "Handshake successful with Kernel Memory");
  return true;
}

static int thread_check_connection_kernel_memory(void* args)
{
  t_connection_check_thread* data = (t_connection_check_thread*)args;
  bool keep_running = true;
  while (keep_running)
  {
    time_sleep_ms(500);
    socket_mutex_lock(data->km_socket);
    keep_running =
        send_string(OP_KERNEL_MEMORY_RUNNING,
                    "Is Kernel Memory still connected?", data->km_socket);
    if (!keep_running)
    {
      close_kernel_scheduler(SR_KERNEL_MEMORY_SEND_ERROR);
    }
    else
    {
      keep_running = !atomic_load(&(data->close));
    }
    socket_mutex_unlock(data->km_socket);
  }
  return 0;
}

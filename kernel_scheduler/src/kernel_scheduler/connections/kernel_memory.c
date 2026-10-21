#include "kernel_scheduler/connections/kernel_memory.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kernel_scheduler/shutdown.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/watchdog.h"

static t_socket* connect_kernel_memory(char* ip, char* port, t_log* logger);
static bool handshake_kernel_memory(t_socket* km_socket, t_log* logger);
static void on_kernel_memory_disconnected(void* arg);

t_socket* start_connection_kernel_memory(char* ip, char* port, t_log* logger)
{
  t_socket* km_socket = connect_kernel_memory(ip, port, logger);
  if (km_socket == NULL)
  {
    return NULL;
  }

  if (!handshake_kernel_memory(km_socket, logger))
  {
    socket_destroy(km_socket);
    return NULL;
  }

  return km_socket;
}

bool notify_terminate_process(t_socket* km_socket, t_shutdown_ctx* shutdown,
                              uint32_t pid)
{
  socket_mutex_lock(km_socket);
  bool ret = send_buffer(OP_END_PROCESS, &pid, sizeof(uint32_t), km_socket);
  if (!ret)
  {
    close_kernel_scheduler(shutdown, SR_KERNEL_MEMORY_SEND_ERROR);
  }
  socket_mutex_unlock(km_socket);
  return ret;
}

t_watchdog* start_thread_check_connection_kernel_memory(
    t_log* logger, t_socket* km_socket, t_shutdown_ctx* shutdown)
{
  t_list* sockets = list_create();
  list_add(sockets, km_socket);

  t_watchdog* watchdog = watchdog_start(
      sockets, NULL, NULL, 500, on_kernel_memory_disconnected, shutdown);
  if (watchdog == NULL)
  {
    log_error(logger,
              "Error creating the connection-check thread for the "
              "Kernel Memory connection");
    list_destroy(sockets);
  }
  return watchdog;
}

void destroy_thread_check_connection_kernel_memory(t_watchdog* watchdog)
{
  t_list* sockets = watchdog->sockets;
  watchdog_destroy(watchdog);
  list_destroy(sockets);
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

static void on_kernel_memory_disconnected(void* arg)
{
  close_kernel_scheduler((t_shutdown_ctx*)arg, SR_KERNEL_MEMORY_SEND_ERROR);
}

#include "kernel_scheduler/kernel_memory.h"

#include <pthread.h>
#include <stdbool.h>

#include "utils/msg.h"

static int connect_kernel_memory(char* ip, char* port, t_log* logger);
static bool handshake_kernel_memory(int km_socket, t_log* logger);
static void* thread_check_connection_kernel_memory(void* arg);

int start_connection_kernel_memory(char* ip, char* port, t_log* logger)
{
  int km_socket = connect_kernel_memory(ip, port, logger);
  if (km_socket <= 0)
    return -1;

  if (!handshake_kernel_memory(km_socket, logger))
    return -1;

  return km_socket;
}

bool notify_terminate_process(t_kernel_memory_socket* km_socket, uint32_t pid,
                              int server_socket, t_log* logger)
{
  pthread_mutex_lock(&(km_socket->socket_mutex));
  bool ret =
      send_buffer(OP_END_PROCESS, &pid, sizeof(uint32_t), km_socket->km_socket);
  if (!ret)
  {
    close_kernel_scheduler(server_socket, logger, SR_KERNEL_MEMORY_SEND_ERROR,
                           km_socket->km_socket);
  }
  pthread_mutex_unlock(&(km_socket->socket_mutex));
  return ret;
}

t_connection_check_thread* start_thread_check_connection_kernel_memory(
    int server_socket, t_log* logger, t_kernel_memory_socket* km_socket)
{
  t_connection_check_thread* data = malloc(sizeof(t_connection_check_thread));
  data->server_socket = server_socket;
  data->logger = logger;
  data->km_socket = km_socket;
  data->close = false;
  pthread_mutex_init(&(data->close_mutex), NULL);

  if (pthread_create(&(data->thread), NULL,
                     thread_check_connection_kernel_memory, data) != 0)
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
  pthread_mutex_lock(&(data->close_mutex));
  data->close = true;
  pthread_mutex_unlock(&(data->close_mutex));
  pthread_join(data->thread, NULL);
  pthread_mutex_destroy(&(data->close_mutex));
  free(data);
}

static int connect_kernel_memory(char* ip, char* port, t_log* logger)
{
  int km_socket = create_connection(ip, port);
  if (km_socket <= 0)
  {
    log_error(logger, "Connection error with Kernel Memory");
    return -1;
  }
  log_info(logger, "## Connected to Kernel Memory");
  return km_socket;
}

static bool handshake_kernel_memory(int km_socket, t_log* logger)
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
  log_info(logger, "Handshake successful with Kernel Memory");
  return true;
}

static void* thread_check_connection_kernel_memory(void* args)
{
  t_connection_check_thread* data = (t_connection_check_thread*)args;
  bool keep_running = true;
  while (keep_running)
  {
    usleep(500000);
    pthread_mutex_lock(&(data->km_socket->socket_mutex));
    keep_running = send_string(OP_KERNEL_MEMORY_RUNNING,
                               "Is Kernel Memory still connected?",
                               data->km_socket->km_socket);
    if (!keep_running)
    {
      close_kernel_scheduler(data->server_socket, data->logger,
                             SR_KERNEL_MEMORY_SEND_ERROR,
                             data->km_socket->km_socket);
    }
    else
    {
      pthread_mutex_lock(&(data->close_mutex));
      keep_running = !data->close;
      pthread_mutex_unlock(&(data->close_mutex));
    }
    pthread_mutex_unlock(&(data->km_socket->socket_mutex));
  }
  return NULL;
}

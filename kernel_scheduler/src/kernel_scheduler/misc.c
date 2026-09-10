#include "kernel_scheduler/misc.h"

#include <pthread.h>
#include <sys/socket.h>
#include <sys/time.h>

#include "utils/msg.h"

const char* const SHUTDOWN_REASONS[4] = {
    "Processes finished successfully", "BSOD: Corruption of memory detected",
    "Connection error with Kernel Memory", "Unknown error"};

static void log_shutdown(t_log* logger, int reason_shutdown);
static void check_reason_shutdown(int* reason_shutdown, int km_socket);
static void notify_shutdown_kernel_memory(int reason_shutdown, int km_socket,
                                          t_log* logger);

bool respond_handshake(int socket_fd, int id_module, t_log* logger)
{
  if (!send_handshake(id_module, socket_fd))
  {
    log_error(logger, "Error sending the handshake to %s",
              HANDSHAKE_MSG[id_module]);
    return false;
  }
  return true;
}

unsigned long millis(void)
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

unsigned long time_diff(unsigned long time_1, unsigned long time_2)
{
  return time_1 > time_2 ? time_1 - time_2 : time_2 - time_1;
}

t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket)
{
  t_process_counter* counter = malloc(sizeof(t_process_counter));
  atomic_init(&(counter->active_process_count), 0);
  counter->server_socket = server_socket;
  counter->logger = logger;
  counter->km_socket = km_socket;
  return counter;
}

void aumentar_counter_processes(t_process_counter* counter)
{
  atomic_fetch_add(&(counter->active_process_count), 1);
}

void disminuir_counter_processes(t_process_counter* counter)
{
  bool is_last = atomic_fetch_sub(&(counter->active_process_count), 1) == 1;

  if (is_last)
  {
    pthread_mutex_lock(&(counter->km_socket->socket_mutex));
    close_kernel_scheduler(counter->server_socket, counter->logger,
                           SR_NO_PROCESSES, counter->km_socket->km_socket);
    pthread_mutex_unlock(&(counter->km_socket->socket_mutex));
  }
}

void destroy_counter_processes(t_process_counter* counter)
{
  free(counter);
}

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

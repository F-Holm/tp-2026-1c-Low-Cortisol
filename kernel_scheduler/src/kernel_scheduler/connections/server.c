#include "kernel_scheduler/connections/server.h"

#include <stdbool.h>

#include "kernel_scheduler/connections/cpu.h"
#include "kernel_scheduler/connections/io.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/mutex.h"
#include "utils/sockets.h"

static void close_thread_listen(t_io* io, t_list* list_sockets_cpu,
                                mtx_t* mutex_list_sockets_cpu,
                                cnd_t* cpu_done_cond,
                                t_listen_server_data* data);

t_socket* create_socket_server(char* port, t_log* logger)
{
  t_socket* ret = socket_create(SOCKET_KIND_SERVER, NULL, port, false);
  if (ret == NULL)
  {
    log_error(logger, "Error creating the server");
    return NULL;
  }
  log_debug(logger, "Server created successfully");
  return ret;
}

void init_data_server_listen(t_listen_server_data* data,
                             t_socket* socket_server, t_log* logger,
                             t_mutex_list* mutex_list, t_queues* queues,
                             t_socket* socket_kernel_memory,
                             char* initial_process_path)
{
  data->socket_server = socket_server;
  data->logger = logger;
  data->mutex_list = mutex_list;
  data->queues = queues;
  data->km_socket = socket_kernel_memory;
  data->initial_process_path = initial_process_path;
}

void server_listen(t_listen_server_data* data)
{
  t_io* io = create_io_structures();
  t_list* list_sockets_cpu = list_create();
  mtx_t mutex_list_sockets_cpu;
  cnd_t cpu_done_cond;

  mtx_init(&mutex_list_sockets_cpu);
  cnd_init(&cpu_done_cond);

  transition_new_ready(data->queues, data->initial_process_path, 0);
  while (true)
  {
    bool handled_ok = true;
    t_socket* socket_fd = socket_accept(data->socket_server, false);
    if (socket_fd == NULL)
    {
      break;
    }

    switch (receive_handshake(socket_fd))
    {
      case MID_CPU:
        handled_ok =
            handle_new_cpu(socket_fd, list_sockets_cpu, &mutex_list_sockets_cpu,
                           &cpu_done_cond, data->logger, data->mutex_list,
                           data->queues, io, data->km_socket);
        break;
      case MID_IO:
        handled_ok = handle_new_io(io, socket_fd, data->queues, false);
        break;
      default:
        log_warning(data->logger, "Invalid handshake received");
        handled_ok = false;
        break;
    }
    if (!handled_ok)
    {
      socket_destroy(socket_fd);
    }
  }

  close_thread_listen(io, list_sockets_cpu, &mutex_list_sockets_cpu,
                      &cpu_done_cond, data);
}

static void close_thread_listen(t_io* io, t_list* list_sockets_cpu,
                                mtx_t* mutex_list_sockets_cpu,
                                cnd_t* cpu_done_cond,
                                t_listen_server_data* data)
{
  log_debug(data->logger, "Closing server");
  close_cpu(list_sockets_cpu, mutex_list_sockets_cpu, cpu_done_cond,
            data->queues);
  close_io(io);
  log_debug(data->logger, "Server closed");
}

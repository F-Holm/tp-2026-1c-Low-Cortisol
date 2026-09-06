#include "kernel_scheduler/server.h"

#include <pthread.h>

#include "kernel_scheduler/cpu.h"
#include "kernel_scheduler/io.h"
#include "kernel_scheduler/misc.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

static void close_thread_listen(t_io* io, t_list* list_sockets_cpu,
                                pthread_mutex_t* mutex_list_sockets_cpu,
                                pthread_cond_t* cond_fin_cpu,
                                t_listen_server_data* data);

int create_socket_server(char* port, t_log* logger)
{
  int ret = start_server(port);
  if (ret <= 0)
  {
    log_error(logger, "Error in the creation of the server");
    return -1;
  }
  log_info(logger, "Server created successfully");
  return ret;
}

void init_data_server_listen(t_listen_server_data* data, int socket_server,
                             t_log* logger, t_mutex_list* mutex_list,
                             t_queues* queues,
                             t_kernel_memory_socket* socket_kernel_memory,
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
  t_io* io = create_estructuras_io();
  t_list* list_sockets_cpu = list_create();
  pthread_mutex_t mutex_list_sockets_cpu;
  pthread_cond_t cond_fin_cpu;

  pthread_mutex_init(&mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&cond_fin_cpu, NULL);

  transition_new_ready(data->queues, data->initial_process_path, 0);
  while (true)
  {
    bool manejo_exitoso = true;
    int socket_fd = accept(data->socket_server, NULL, NULL);
    if (socket_fd <= 0)
    {
      break;
    }

    switch (receive_handshake(socket_fd))
    {
      case MID_CPU:
        manejo_exitoso = handle_new_cpu(
            socket_fd, list_sockets_cpu, &mutex_list_sockets_cpu, &cond_fin_cpu,
            data->logger, data->mutex_list, data->queues, io, data->km_socket,
            data->socket_server);
        break;
      case MID_IO:
        manejo_exitoso = handle_new_io(io, socket_fd, data->queues, false,
                                       data->socket_server);
        break;
      default:
        log_info(data->logger, "Invalid handshake received");
        manejo_exitoso = false;
        break;
    }
    if (!manejo_exitoso)
    {
      close(socket_fd);
    }
  }

  close_thread_listen(io, list_sockets_cpu, &mutex_list_sockets_cpu,
                      &cond_fin_cpu, data);
}

static void close_thread_listen(t_io* io, t_list* list_sockets_cpu,
                                pthread_mutex_t* mutex_list_sockets_cpu,
                                pthread_cond_t* cond_fin_cpu,
                                t_listen_server_data* data)
{
  log_info(data->logger, "Closing server");
  close_cpu(list_sockets_cpu, mutex_list_sockets_cpu, cond_fin_cpu,
            data->queues);
  close_io(io);
  log_info(data->logger, "Server closed");
}

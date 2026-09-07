#include "kernel_memory/server.h"

bool handshake(t_kernel_memory_data* kernel_data, int client_socket)
{
  static int socket_scheduler = -1;
  if (client_socket != -1)
  {
    log_trace(kernel_data->logger, "A client was accepted!");
    log_trace(kernel_data->logger, "Server waiting for handshake");
  }

  int identifier = receive_handshake(client_socket);
  switch (identifier)
  {
    case MID_KERNEL_SCHEDULER:
    {
      if (!send_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        send_handshake_error(kernel_data->logger, client_socket,
                             "Kernel Scheduler");
        return false;
      }
      log_info(kernel_data->logger,
               "Kernel Scheduler Connected - FD of the socket: %i",
               client_socket);
      t_scheduler_data* scheduler_data = init_scheduler_data(
          kernel_data->socket_kernel_memory, client_socket,
          kernel_data->processes, kernel_data->scripts_basepath,
          kernel_data->processes_mutex, kernel_data->main_memory,
          kernel_data->connected_sticks, kernel_data->socket_list_mutex,
          kernel_data->swap_data, kernel_data->logger,
          &kernel_data->active_threads, kernel_data->active_threads_mutex,
          kernel_data->active_threads_cond);
      pthread_mutex_lock(kernel_data->active_threads_mutex);
      kernel_data->active_threads++;
      pthread_mutex_unlock(kernel_data->active_threads_mutex);
      kernel_data->socket_scheduler = client_socket;
      start_scheduler_listener(scheduler_data);
      socket_scheduler = client_socket;
    }
    break;

    case MID_CPU:
    {
      if (socket_scheduler == -1)
      {
        log_debug(kernel_data->logger,
                  "Kernel Scheduler not connected, connection rejected: %i",
                  client_socket);
        close(client_socket);
        break;
      }
      if (!send_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        send_handshake_error(kernel_data->logger, client_socket, "CPU");
        return false;
      }
      bool init_ok = true;
      log_debug(kernel_data->logger, "A CPU connected!");
      t_cpu_data* cpu_data = init_cpu_data(
          client_socket, kernel_data->processes, kernel_data->processes_mutex,
          kernel_data->instruction_delay, kernel_data->main_memory,
          kernel_data->logger, &kernel_data->active_threads,
          kernel_data->active_threads_mutex, kernel_data->active_threads_cond,
          socket_scheduler);
      init_ok = receive_cpu_id(cpu_data);
      send_buffer(OP_MAX_SEGMENT_SIZE, &kernel_data->segment_max_size,
                  sizeof(int), cpu_data->socket_cpu);
      add_cpu_connection(kernel_data, cpu_data);
      if (init_ok)
      {
        send_connected_sticks(kernel_data->connected_sticks,
                              kernel_data->socket_list_mutex, cpu_data);
        pthread_mutex_lock(kernel_data->active_threads_mutex);
        kernel_data->active_threads++;
        pthread_mutex_unlock(kernel_data->active_threads_mutex);
        start_cpu_listener(cpu_data);
      }
      else
      {
        send_init_error(kernel_data->logger, client_socket, "CPU");
      }
    }
    break;

    case MID_MEMORY_STICK:
    {
      if (!send_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        send_handshake_error(kernel_data->logger, client_socket,
                             "Memory Stick");
        return false;
      }
      log_debug(kernel_data->logger, "A memory stick connected!");
      bool init_ok = true;
      t_stick_data* stick_data = init_stick_data(
          client_socket, kernel_data->logger, kernel_data->socket_scheduler);
      init_ok = resolve_stick_ip(stick_data, client_socket);
      init_ok = receive_stick_size(stick_data);
      init_ok = receive_stick_listen_port(stick_data);
      add_stick_connection(kernel_data, stick_data);
      if (init_ok)
      {
        send_cpu_connection(stick_data, kernel_data->connected_cpus);
        send_string(OP_NEW_MEMORY_STICK, "A new memory stick connected",
                    kernel_data->socket_scheduler);
        add_total_memory(kernel_data->main_memory, stick_data->stick_size);
      }
      else
      {
        send_init_error(kernel_data->logger, stick_data->socket_stick,
                        "Memory Stick");
      }
    }
    break;

    case MID_SWAP:
    {
      if (!send_handshake(MID_KERNEL_MEMORY, client_socket))
      {
        send_handshake_error(kernel_data->logger, client_socket, "SWAP");
        return false;
      }
      log_debug(kernel_data->logger, "SWAP connected");
      t_swap_data* swap_data =
          init_swap_data(client_socket, kernel_data->logger);
      kernel_data->swap_data = swap_data;
      break;
    }
    default:
      return false;
      break;
  }
  return true;
}

bool accept_client(void* ptr)
{
  t_kernel_memory_data* kernel_data = (t_kernel_memory_data*)ptr;
  log_trace(kernel_data->logger, "Server waiting for a client");
  int client_socket = accept(kernel_data->socket_kernel_memory, NULL, NULL);
  return handshake(kernel_data, client_socket);
}

#include "memory_stick/cpu.h"

#include <bits/pthreadtypes.h>
#include <pthread.h>

#include "utils/msg.h"

int create_server_cpu(t_log* logger)
{
  int ret = start_server("0");
  if (ret <= 0)
  {
    log_error(logger, "## Error creating the CPU server");
    return -1;
  }
  log_debug(logger, "CPU server created successfully");
  return ret;
}

uint16_t get_cpu_port(int socket_server_cpu)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(socket_server_cpu, (struct sockaddr*)&addr, &len);
  return ntohs(addr.sin_port);
}

void iterator_shutdown(void* value)
{
  shutdown(*(int*)value, SHUT_RDWR);
}

t_cpu_thread* create_cpu_thread_data(int socket_cpu, t_list* socket_list,
                                     pthread_mutex_t* socket_list_mutex,
                                     pthread_cond_t* listen_done_cond, t_ms* ms)
{
  t_cpu_thread* data = malloc(sizeof(t_cpu_thread));
  data->socket_cpu = socket_cpu;
  data->socket_list = socket_list;
  data->socket_list_mutex = socket_list_mutex;
  data->listen_done_cond = listen_done_cond;
  data->ms = ms;
  return data;
}

bool spawn_cpu_thread(t_cpu_thread* cpu_thread, t_log* logger)
{
  pthread_t thread;
  if (pthread_create(&thread, NULL, handle_cpu_client, cpu_thread) != 0)
  {
    log_error(logger, "## Could not create the CPU thread");
    return false;
  }
  pthread_detach(thread);
  return true;
}

void close_listen_thread(t_list* socket_list,
                         pthread_mutex_t* socket_list_mutex,
                         pthread_cond_t* listen_done_cond,
                         t_listen_thread* listen_thread)
{
  pthread_mutex_lock(socket_list_mutex);
  list_iterate(socket_list, (void*)iterator_shutdown);
  while (!list_is_empty(socket_list))
    pthread_cond_wait(listen_done_cond, socket_list_mutex);
  pthread_mutex_unlock(socket_list_mutex);
  list_destroy(socket_list);
  pthread_cond_destroy(listen_done_cond);
  pthread_mutex_destroy(socket_list_mutex);
  free(listen_thread);
}

bool handshake_cpu(int socket_cpu, t_log* logger)
{
  if (receive_handshake(socket_cpu) != MID_CPU)
  {
    log_warning(logger, "## Could not receive the handshake from the CPU");
    return false;
  }
  if (!send_handshake(MID_MEMORY_STICK, socket_cpu))
  {
    log_warning(logger, "## Could not send the handshake to the CPU");
    return false;
  }
  log_debug(logger, "Handshake successful with the CPU");
  return true;
}

char* receive_cpu_id(int socket_cpu, t_log* logger)
{
  if (receive_op_code(socket_cpu) != OP_ID_CPU)
  {
    log_warning(logger, "## Could not receive the CPU ID");
    return NULL;
  }
  char* cpu_id = receive_string(socket_cpu);
  log_info(logger, "## CPU %s connected", cpu_id);
  return cpu_id;
}

bool handle_new_cpu(t_listen_thread* listen_thread, int socket_cpu,
                    t_list* socket_list, pthread_mutex_t* socket_list_mutex,
                    pthread_cond_t* listen_done_cond, t_ms* ms)
{
  if (!handshake_cpu(socket_cpu, listen_thread->logger))
    return false;

  char* cpu_id = receive_cpu_id(socket_cpu, listen_thread->logger);
  if (cpu_id == NULL)
    return false;
  free(cpu_id);

  t_cpu_thread* cpu_thread = create_cpu_thread_data(
      socket_cpu, socket_list, socket_list_mutex, listen_done_cond, ms);

  pthread_mutex_lock(socket_list_mutex);
  list_add(socket_list, &(cpu_thread->socket_cpu));
  pthread_mutex_unlock(socket_list_mutex);

  if (!spawn_cpu_thread(cpu_thread, listen_thread->logger))
  {
    pthread_mutex_lock(socket_list_mutex);
    list_remove_element(socket_list, &(cpu_thread->socket_cpu));
    pthread_mutex_unlock(socket_list_mutex);
    free(cpu_thread);
    return false;
  }
  return true;
}

void* cpu_listen_thread(void* listen_thread_void)
{
  t_listen_thread* listen_thread = (t_listen_thread*)listen_thread_void;

  t_list* socket_list = list_create();
  pthread_mutex_t socket_list_mutex;
  pthread_cond_t listen_done_cond;

  pthread_mutex_init(&socket_list_mutex, NULL);
  pthread_cond_init(&listen_done_cond, NULL);

  while (true)
  {
    int socket_cpu = accept(listen_thread->cpu_listen_socket, NULL, NULL);
    if (socket_cpu <= 0)
      break;

    log_debug(listen_thread->logger, "Connection established with a CPU");

    if (!handle_new_cpu(listen_thread, socket_cpu, socket_list,
                        &socket_list_mutex, &listen_done_cond,
                        listen_thread->ms))
      close(socket_cpu);
  }

  log_debug(listen_thread->logger, "Closing the CPU server");
  close_listen_thread(socket_list, &socket_list_mutex, &listen_done_cond,
                      listen_thread);
  return NULL;
}

void close_cpu_thread(t_cpu_thread* cpu_thread)
{
  close(cpu_thread->socket_cpu);
  pthread_mutex_lock(cpu_thread->socket_list_mutex);
  list_remove_element(cpu_thread->socket_list, &(cpu_thread->socket_cpu));
  if (list_is_empty(cpu_thread->socket_list))
    pthread_cond_signal(cpu_thread->listen_done_cond);
  pthread_mutex_unlock(cpu_thread->socket_list_mutex);
  free(cpu_thread);
}

void* handle_cpu_client(void* cpu_thread_void)
{
  t_cpu_thread* cpu_thread = (t_cpu_thread*)cpu_thread_void;

  while (true)
  {
    int op_code = receive_op_code(cpu_thread->socket_cpu);
    switch (op_code)
    {
      case OP_MEMORY_STICK_READ:
      {
        log_trace(cpu_thread->ms->logger,
                  "Receiving a read instruction from the CPU");
        t_list* packet = receive_packet(cpu_thread->socket_cpu);
        if (list_size(packet) != 2)
        {
          log_error(cpu_thread->ms->logger,
                    "## Invalid read request from the CPU");
          break;
        }
        int start_position = *(int*)list_get(packet, 0);
        int byte_count = *(int*)list_get(packet, 1);
        log_trace(cpu_thread->ms->logger,
                  "Read of %d bytes, from %d, requested by the CPU", byte_count,
                  start_position);
        list_destroy_and_destroy_elements(packet, free);
        read_memory(cpu_thread->ms, start_position, byte_count,
                    cpu_thread->socket_cpu);
        log_info(cpu_thread->ms->logger, "## Read of %d bytes", byte_count);
        break;
      }
      case OP_MEMORY_STICK_WRITE:
      {
        log_trace(cpu_thread->ms->logger,
                  "Receiving a write instruction from the CPU");
        t_list* packet = receive_packet(cpu_thread->socket_cpu);
        if (list_size(packet) != 3)
        {
          log_error(cpu_thread->ms->logger,
                    "## Invalid write request from the CPU");
          list_destroy_and_destroy_elements(packet, free);
          break;
        }
        int start_position = *(int*)list_get(packet, 0);
        char* bytes_to_write = (char*)list_get(packet, 1);
        int byte_count = *(int*)list_get(packet, 2);
        write_memory(cpu_thread->ms, start_position, bytes_to_write, byte_count,
                     cpu_thread->socket_cpu);
        log_info(cpu_thread->ms->logger, "## Write of %d bytes", byte_count);
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      default:
        close_cpu_thread(cpu_thread);
        return NULL;
    }
  }

  close_cpu_thread(cpu_thread);
  return NULL;
}

bool start_cpu_server(pthread_t* cpu_server_thread, int cpu_server_socket,
                      t_log* logger, t_ms* ms)
{
  t_listen_thread* listen_thread = malloc(sizeof(t_listen_thread));
  listen_thread->cpu_listen_socket = cpu_server_socket;
  listen_thread->logger = logger;
  listen_thread->ms = ms;
  if (pthread_create(cpu_server_thread, NULL, cpu_listen_thread,
                     listen_thread) != 0)
  {
    log_error(logger, "## Could not create the CPU server thread");
    return false;
  }
  return true;
}

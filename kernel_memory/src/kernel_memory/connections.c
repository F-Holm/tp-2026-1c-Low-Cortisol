#include "kernel_memory/connections.h"

#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/registry.h"
#include "utils/msg.h"

bool receive_cpu_id(t_cpu_data* cpu_data)
{
  if (receive_op_code(cpu_data->socket_cpu) == OP_ID_CPU)
  {
    char* id_cpu = receive_string(cpu_data->socket_cpu);
    log_info(cpu_data->logger, "CPU %s connected", id_cpu);
    cpu_data->id = atoi(id_cpu);
    free(id_cpu);
    return true;
  }
  else
  {
    log_debug(cpu_data->logger,
              "Could not connect to the CPU because it could not "
              "sent the ID operation");
    close_communication(cpu_data->socket_cpu);
    return false;
  }
  return false;
}

bool receive_stick_size(t_stick_data* stick_data)
{
  if (receive_op_code(stick_data->socket_stick) == OP_MEMORY_SIZE)
  {
    char* size = receive_string(stick_data->socket_stick);
    log_info(stick_data->logger, "Memory Stick of %s bytes connected", size);
    stick_data->stick_size = atoi(size);
    free(size);
    return true;
  }
  else
  {
    log_debug(stick_data->logger,
              "Could not connect to the stick because it could not "
              "sent the size operation");
    close_communication(stick_data->socket_stick);
    return false;
  }
  return false;
}

bool receive_stick_listen_port(t_stick_data* stick_data)
{
  if (receive_op_code(stick_data->socket_stick) == OP_PORT)
  {
    char* port = receive_string(stick_data->socket_stick);
    log_debug(stick_data->logger, "Memory Stick port received %s", port);
    stick_data->stick_port = atoi(port);
    free(port);
    return true;
  }
  else
  {
    log_debug(stick_data->logger,
              "Could not connect to the stick because it could not "
              "sent the port operation");
    close_communication(stick_data->socket_stick);
    return false;
  }
  return false;
}

void add_stick_connection(t_kernel_memory_data* kernel_data,
                          t_stick_data* stick_data)
{
  list_add_mtx(kernel_data->connected_sticks, kernel_data->socket_list_mutex,
               stick_data);
  return;
}

void add_cpu_connection(t_kernel_memory_data* kernel_data, t_cpu_data* cpu_data)
{
  list_add_mtx(kernel_data->connected_cpus, kernel_data->socket_list_mutex,
               cpu_data);
  return;
}

void send_connected_sticks(t_list* connected_sticks,
                           pthread_mutex_t* socket_list_mutex,
                           t_cpu_data* cpu_data)
{
  pthread_mutex_lock(socket_list_mutex);
  int total_sticks = list_size(connected_sticks);

  for (int i = 0; i < total_sticks; i++)
  {
    t_packet* packet = create_packet(OP_PACKET);
    t_stick_data* current_stick = (t_stick_data*)list_get(connected_sticks, i);

    char port[6];
    snprintf(port, sizeof(port), "%u", current_stick->stick_port);

    packet_append_string(packet, current_stick->ip_memory_stick);
    packet_append_string(packet, port);
    packet_append(packet, &current_stick->stick_size, sizeof(int));

    send_packet(packet, cpu_data->socket_cpu);
    destroy_packet(packet);
  }
  pthread_mutex_unlock(socket_list_mutex);
}

void send_cpu_connection(t_stick_data* stick_data, t_list* connected_cpus)
{
  if (list_is_empty(connected_cpus))
  {
    return;
  }
  t_packet* packet = create_packet(OP_PACKET);

  packet_append_string(packet, stick_data->ip_memory_stick);

  char port[6];
  snprintf(port, sizeof(port), "%u", stick_data->stick_port);
  packet_append_string(packet, port);
  packet_append(packet, &stick_data->stick_size, sizeof(int));

  for (int i = 0; i < list_size(connected_cpus); i++)
  {
    t_cpu_data* current_cpu = (t_cpu_data*)list_get(connected_cpus, i);
    send_packet(packet, current_cpu->socket_cpu);
  }
  destroy_packet(packet);
}

int compute_total_memory(t_list* connected_sticks,
                         pthread_mutex_t* socket_list_mutex)
{
  int total = 0;
  for (int i = 0; i < list_size(connected_sticks); i++)
  {
    pthread_mutex_lock(socket_list_mutex);
    t_stick_data* current_stick = (t_stick_data*)list_get(connected_sticks, i);
    pthread_mutex_unlock(socket_list_mutex);
    total += current_stick->stick_size;
  }
  return total;
}

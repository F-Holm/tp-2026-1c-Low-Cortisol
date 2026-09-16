#include "kernel_memory/connections.h"

#include <stdio.h>
#include <stdlib.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/registry.h"
#include "utils/msg.h"

// Reads one handshake field shaped as "an expected op-code, then an int sent
// as a string": receive_cpu_id/receive_stick_size/receive_stick_listen_port
// are all this same shape, differing only in the op-code, the field's name
// for logging, and where the parsed value is stored.
static bool receive_int_field(int socket, t_log* logger, int expected_op,
                              const char* field_name, int* out_value)
{
  if (receive_op_code(socket) != expected_op)
  {
    log_debug(logger,
              "Could not connect because the peer did not send the %s "
              "operation",
              field_name);
    close_communication(socket);
    return false;
  }
  char* value = receive_string(socket);
  log_debug(logger, "%s received: %s", field_name, value);
  *out_value = atoi(value);
  free(value);
  return true;
}

bool receive_cpu_id(t_cpu_data* cpu_data)
{
  return receive_int_field(cpu_data->socket_cpu, cpu_data->logger, OP_ID_CPU,
                           "CPU id", &cpu_data->id);
}

bool receive_stick_size(t_stick_data* stick_data)
{
  return receive_int_field(stick_data->socket_stick, stick_data->logger,
                           OP_MEMORY_SIZE, "Memory Stick size",
                           &stick_data->stick_size);
}

bool receive_stick_listen_port(t_stick_data* stick_data)
{
  return receive_int_field(stick_data->socket_stick, stick_data->logger,
                           OP_PORT, "Memory Stick port",
                           &stick_data->stick_port);
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

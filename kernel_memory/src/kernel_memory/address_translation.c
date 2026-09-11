#include "kernel_memory/address_translation.h"

#include <stdlib.h>
#include <string.h>

#include "kernel_memory/segments.h"
#include "utils/msg.h"

int translate_logical_address(uint32_t pid, uint32_t logical_address,
                              uint32_t size, t_main_memory* main_memory,
                              t_log* logger)
{
  // Compute the physical address
  int seg_max = main_memory->max_segment_size;
  uint32_t segment_number = logical_address / seg_max;
  uint32_t offset_in_segment = logical_address % seg_max;

  t_segment* found_seg = find_segment(main_memory, pid, segment_number);
  if (found_seg == NULL)
  {
    log_error(logger, "Segment number %u was not found for process %u",
              segment_number, pid);
    return -1;
  }
  int physical_address = found_seg->base + offset_in_segment;
  return physical_address;
}

int find_stick(int physical_address, t_list* connected_sticks,
               pthread_mutex_t* sticks_mutex, int* stick_offset)
{
  int accumulated_base = 0;
  int index = -1;
  int current_index = 0;

  pthread_mutex_lock(sticks_mutex);
  t_list_iterator* iterator = list_iterator_create(connected_sticks);
  while (list_iterator_has_next(iterator))
  {
    t_stick_data* current_item = list_iterator_next(iterator);
    if (physical_address >= accumulated_base &&
        physical_address < accumulated_base + current_item->stick_size)
    {
      // found the stick for the physical address
      *stick_offset = physical_address - accumulated_base;
      index = current_index;
      break;
    }
    current_index++;
    accumulated_base += current_item->stick_size;
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(sticks_mutex);
  return index;
}

char* read_from_sticks(int physical_address, int size, t_list* connected_sticks,
                       pthread_mutex_t* sticks_mutex, t_log* logger,
                       int socket_scheduler)
{
  char* result = malloc(size);
  int bytes_read = 0;
  int current_address = physical_address;
  log_trace(logger, "Reading %d bytes from physical_address %d", size,
            physical_address);
  while (bytes_read < size)
  {
    int stick_offset = 0;
    int index = find_stick(current_address, connected_sticks, sticks_mutex,
                           &stick_offset);
    if (index == -1)
    {
      log_error(logger,
                "The computed physical address does not match any "
                "connected stick");
      free(result);
      return NULL;
    }
    pthread_mutex_lock(sticks_mutex);
    t_stick_data* stick = list_get(connected_sticks, index);
    int bytes_to_stick_end = stick->stick_size - stick_offset;
    int bytes_to_read_count = size - bytes_read;
    // check whether the requested size fits in the stick or spills past its end
    if (bytes_to_read_count > bytes_to_stick_end)
      bytes_to_read_count = bytes_to_stick_end;
    // Send read request to the stick
    t_packet* packet = create_packet(OP_MEMORY_STICK_READ);
    packet_append(packet, &stick_offset, sizeof(int));
    packet_append(packet, &bytes_to_read_count, sizeof(int));
    if (!send_packet(packet, stick->socket_stick))
    {
      log_warning(logger, "Error sending read packet to stick %d", index);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      free(result);
      pthread_mutex_unlock(sticks_mutex);
      return NULL;
    }
    destroy_packet(packet);
    pthread_mutex_unlock(sticks_mutex);

    // Receive the response
    int stick_socket =
        ((t_stick_data*)list_get(connected_sticks, index))->socket_stick;
    int op = receive_op_code(stick_socket);
    if (op == 0)
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }
    if (op != OP_MEMORY_STICK_READ_DONE)
    {
      log_warning(logger, "Wrong response opcode from stick %d", index);
      free(result);
      return NULL;
    }
    // receive the bytes as a string
    int received_size = 0;
    char* fragment = receive_buffer(&received_size, stick_socket);

    memcpy(result + bytes_read, fragment, bytes_to_read_count);
    free(fragment);

    bytes_read += bytes_to_read_count;
    current_address += bytes_to_read_count;
  }

  log_trace(logger, "Read %d bytes from physical_address %d", size,
            physical_address);
  return result;
}

bool write_to_sticks(int pid, int physical_address, int bytes_to_read,
                     char* write_buffer, t_list* connected_sticks,
                     pthread_mutex_t* socket_list_mutex, t_log* logger,
                     int socket_scheduler)
{
  int stick_offset = 0;
  int index = find_stick(physical_address, connected_sticks, socket_list_mutex,
                         &stick_offset);

  pthread_mutex_lock(socket_list_mutex);

  int remaining = bytes_to_read;
  char* buffer_pointer = write_buffer;
  int current_offset = stick_offset;
  bool ok = true;

  for (int i = index; remaining > 0; i++)
  {
    t_stick_data* current_stick = list_get(connected_sticks, i);
    if (current_stick == NULL)
    {
      log_warning(logger,
                  "No more sticks available to finish the write "
                  "(PID: %d, Phys. Addr: %d)",
                  pid, physical_address);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      ok = false;
      break;
    }

    int available_space = current_stick->stick_size - current_offset;
    int to_write = remaining < available_space ? remaining : available_space;

    log_trace(logger, "Starting to write to stick %d, offset %d", i,
              current_offset);

    t_packet* packet = create_packet(OP_MEMORY_STICK_WRITE);
    packet_append(packet, &current_offset, sizeof(int));
    packet_append(packet, buffer_pointer, to_write);
    packet_append(packet, &to_write, sizeof(int));

    if (!send_packet(packet, current_stick->socket_stick))
    {
      log_warning(logger, "Error sending write packet to stick %d", i);
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
      destroy_packet(packet);
      ok = false;
      break;
    }
    destroy_packet(packet);

    log_trace(logger, "PID: %d - Write - Phys. Addr: %d - Size: %d", pid,
              physical_address, to_write);

    if (receive_op_code(current_stick->socket_stick) ==
        OP_MEMORY_STICK_WRITE_DONE)
    {
      char* buffer = receive_string(current_stick->socket_stick);
      free(buffer);
    }
    else
    {
      send_string(OP_MEMORY_CORRUPTED, "Stick not available", socket_scheduler);
    }

    buffer_pointer += to_write;
    remaining -= to_write;
    current_offset = 0; /* from the second stick onwards it always writes from
                          the start */
  }

  pthread_mutex_unlock(socket_list_mutex);
  return ok;
}

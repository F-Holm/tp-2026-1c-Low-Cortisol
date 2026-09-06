#include "cpu/memory.h"

#include <stdio.h>

#include "cpu/cleanup.h"
#include "cpu/connections.h"
#include "cpu/cpu.h"
#include "cpu/registers.h"

uint32_t mmu(t_cpu* cpu, t_context* context, uint32_t logical_address,
             uint32_t size, uint32_t pid)
{
  uint32_t segment_number = logical_address / cpu->max_segment_size;
  uint32_t offset = logical_address % cpu->max_segment_size;

  t_segmento* segment =
      find_segment_by_id(context->segment_table, segment_number);

  if (segment == NULL)
  {
    log_error(cpu->logger, "## Segment %u not found", segment_number);
    return INVALID_ADDRESS - 1;
  }

  if (offset + size > segment->size)
  {
    if (notify_seg_fault(cpu, pid))
      return INVALID_ADDRESS;
    else
      return INVALID_ADDRESS - 1;
  }

  return segment->base + offset;
}

t_segmento* find_segment_by_id(t_list* segment_table, uint32_t segment_number)
{
  t_segmento* segment = NULL;
  for (int i = 0; i < list_size(segment_table); i++)
  {
    segment = list_get(segment_table, i);
    if (segment->id == segment_number)
    {
      return segment;
    }
  }
  return NULL;
}

bool notify_seg_fault(t_cpu* cpu, uint32_t pid)
{
  if (!enviar_string(OP_SEG_FAULT, "SEGMENTATION FAULT",
                     cpu->socket_kernel_scheduler))
  {
    log_error(cpu->logger, "## Failed to send the segmentation fault");
    return false;
  }
  log_info(cpu->logger, "Segmentation fault sent successfully");
  return true;
}

t_memory_stick_info* find_stick(t_cpu* cpu, uint32_t physical_address)
{
  for (int i = 0; i < list_size(cpu->memory_sticks); i++)
  {
    t_memory_stick_info* stick = list_get(cpu->memory_sticks, i);
    if (physical_address >= stick->offset &&
        physical_address < stick->offset + stick->size)
      return stick;
  }
  return NULL;
}

void* read_memory(t_cpu* cpu, uint32_t physical_address, uint32_t size)
{
  void* result = malloc(size);
  uint32_t read_bytes = 0;

  while (read_bytes < size)
  {
    t_memory_stick_info* stick = find_stick(cpu, physical_address + read_bytes);
    if (stick == NULL)
    {
      log_error(cpu->logger, "## Memory Stick not found");
      free(result);
      return NULL;
    }

    uint32_t address_in_stick = (physical_address + read_bytes) - stick->offset;
    uint32_t available_bytes = stick->size - address_in_stick;
    uint32_t bytes_to_read;

    // Use this stick or move on to the next one.
    if (available_bytes < (size - read_bytes))
      bytes_to_read = available_bytes;
    else
      bytes_to_read = size - read_bytes;

    if (!request_read(cpu, stick, address_in_stick, bytes_to_read))
      return NULL;

    char* partial_read = receive_read_response(cpu, stick);

    if (!partial_read)
    {
      log_error(cpu->logger, "## Null read from the MS");
      free(partial_read);
      free(result);
      return NULL;
    }
    memcpy(result + read_bytes, partial_read, bytes_to_read);
    free(partial_read);

    read_bytes += bytes_to_read;
  }
  return result;
}

bool request_read(t_cpu* cpu, t_memory_stick_info* stick,
                  uint32_t address_in_stick, uint32_t bytes_to_read)
{
  t_paquete* packet = crear_paquete(OP_MEMORY_STICK_LEER);
  agregar_a_paquete(packet, &address_in_stick, sizeof(uint32_t));
  agregar_a_paquete(packet, &bytes_to_read, sizeof(uint32_t));

  if (!enviar_paquete(packet, stick->socket_ms))
  {
    log_error(cpu->logger, "## Error sending the read request to the MS");
    notify_bsod(cpu);
    return false;
  }
  log_info(cpu->logger, "Read requested from the MS");
  eliminar_paquete(packet);
  return true;
}

char* receive_read_response(t_cpu* cpu, t_memory_stick_info* stick)
{
  int op_code = recibir_operacion(stick->socket_ms);
  if (op_code == OP_MEMORY_STICK_LEIDO)
  {
    log_info(cpu->logger, "Read done");
    return recibir_string(stick->socket_ms);
  }
  else
  {
    log_error(cpu->logger, "## Did not receive the read response correctly");

    notify_bsod(cpu);
    return NULL;
  }
}

bool write_memory(t_cpu* cpu, uint32_t physical_address, void* data_to_write,
                  uint32_t size)
{
  uint32_t written_bytes = 0;

  while (written_bytes < size)
  {
    t_memory_stick_info* stick =
        find_stick(cpu, physical_address + written_bytes);
    if (stick == NULL)
    {
      log_error(cpu->logger, "## Memory Stick not found");
      return false;
    }

    uint32_t address_in_stick =
        (physical_address + written_bytes) - stick->offset;
    uint32_t available_bytes = stick->size - address_in_stick;
    uint32_t bytes_to_write;

    if (available_bytes < (size - written_bytes))
      bytes_to_write = available_bytes;
    else
      bytes_to_write = size - written_bytes;

    if (!request_write(cpu, stick, address_in_stick,
                       data_to_write + written_bytes, bytes_to_write))
      return false;

    if (!receive_write_response(cpu, stick))
      return false;

    written_bytes += bytes_to_write;
  }
  return true;
}

bool request_write(t_cpu* cpu, t_memory_stick_info* stick,
                   uint32_t address_in_stick, void* data,
                   uint32_t bytes_to_write)
{
  t_paquete* packet = crear_paquete(OP_MEMORY_STICK_ESCRIBIR);
  agregar_a_paquete(packet, &address_in_stick, sizeof(uint32_t));
  agregar_a_paquete(packet, data, bytes_to_write);
  agregar_a_paquete(packet, &bytes_to_write, sizeof(uint32_t));
  if (!enviar_paquete(packet, stick->socket_ms))
  {
    log_error(cpu->logger, "## Error sending the write request to the MS");
    notify_bsod(cpu);
    eliminar_paquete(packet);
    return false;
  }
  log_info(cpu->logger, "Write requested from the MS");
  eliminar_paquete(packet);
  return true;
}

bool receive_write_response(t_cpu* cpu, t_memory_stick_info* stick)
{
  int op_code = recibir_operacion(stick->socket_ms);
  free(recibir_string(stick->socket_ms));
  if (op_code == OP_MEMORY_STICK_ESCRITO)
  {
    log_info(cpu->logger, "Write done");
    return true;
  }
  else if (op_code == OP_CODE_ERROR)
  {
    log_error(cpu->logger, "## MS disconnected");
    notify_bsod(cpu);
    return false;
  }
  log_error(cpu->logger, "## Did not receive the write response correctly");
  return false;
}

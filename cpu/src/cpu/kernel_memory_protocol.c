#include "cpu/kernel_memory_protocol.h"

#include <stdlib.h>
#include <string.h>

#include "cpu/connections.h"
#include "utils/log.h"

bool receive_max_segment_size(t_cpu* cpu)
{
  int op_code = receive_op_code(cpu->socket_kernel_memory);
  if (op_code == OP_MAX_SEGMENT_SIZE)
  {
    int size;
    void* buffer = receive_buffer(&size, cpu->socket_kernel_memory);
    cpu->max_segment_size = *(int*)buffer;
    free(buffer);
    log_debug(cpu->logger, "Maximum segment size received: %u",
              cpu->max_segment_size);
  }
  else
  {
    log_error(cpu->logger, "Wrong operation code: %d", op_code);
    int size;
    free(receive_buffer(&size, cpu->socket_kernel_memory));
    return false;
  }
  return true;
}

bool parse_stick_packet(t_cpu* cpu, t_list* packet, char stick_ip[16],
                        char stick_port[6], uint32_t* size)
{
  if (list_size(packet) != 3)
  {
    log_error(cpu->logger,
              "Bad memory stick packet: got %d fields, expected 3 (ip, port, "
              "size)",
              list_size(packet));
    list_destroy_and_destroy_elements(packet, free);
    return false;
  }
  strcpy(stick_ip, list_get(packet, 0));
  strcpy(stick_port, list_get(packet, 1));
  *size = *(int*)list_get(packet, 2);
  list_destroy_and_destroy_elements(packet, free);
  log_debug(cpu->logger, "Memory Stick IP: %s | Port: %s", stick_ip,
            stick_port);
  return true;
}

bool listen_kernel_memory(t_cpu* cpu)
{
  bool keep_going = true;
  while (keep_going)
  {
    int op_code = receive_op_code(cpu->socket_kernel_memory);
    switch (op_code)
    {
      case OP_SEGMENT_TABLE:
        keep_going = false;
        break;

      case OP_SEND_CONTEXT:
        keep_going = false;
        break;

      case OP_SEND_INSTRUCTION:
        keep_going = false;
        break;

      case OP_PACKET:
        if (!connect_memory_stick(cpu))
          return false;
        break;

      case OP_CODE_ERROR:
        log_warning(cpu->logger, "Kernel Memory disconnected");
        return false;

      default:
        log_warning(cpu->logger, "Unrecognized operation code: %d", op_code);
        return false;
    }
  }
  return true;
}

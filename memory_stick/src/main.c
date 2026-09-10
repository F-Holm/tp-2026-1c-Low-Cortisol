#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "memory_stick/kernel_memory.h"
#include "memory_stick/memory_stick.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_ms ms = {0};
  pthread_t cpu_server_thread;

  char* config_path = NULL;
  char* size_str = NULL;
  int size;
  if (!get_args(argc, argv, &config_path, &size_str, &size))
    return EXIT_FAILURE;

  if (!init_module(&ms, config_path, size_str, &cpu_server_thread))
  {
    close_module_on_error(&ms);
    return EXIT_FAILURE;
  }

  // Waiting for instructions from Kernel Memory.
  bool keep_running = true;
  while (keep_running)
  {
    int op_code = receive_op_code(ms.socket_km);
    switch (op_code)
    {
      case OP_MEMORY_STICK_READ:
      {
        log_trace(ms.logger, "Receiving a read instruction from Kernel Memory");
        t_list* packet = receive_packet(ms.socket_km);
        if (list_size(packet) != 2)
        {
          log_error(ms.logger, "Invalid read request from Kernel Memory");
          list_destroy_and_destroy_elements(packet, free);
          break;
        }
        int start_position = *(int*)list_get(packet, 0);
        int byte_count = *(int*)list_get(packet, 1);
        read_memory(&ms, start_position, byte_count, ms.socket_km);
        log_info(ms.logger, "Read of %d bytes", byte_count);
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      case OP_MEMORY_STICK_WRITE:
      {
        log_trace(ms.logger,
                  "Receiving a write instruction from Kernel Memory");
        t_list* packet = receive_packet(ms.socket_km);
        if (list_size(packet) != 3)
        {
          log_error(ms.logger, "Invalid write request from Kernel Memory");
          list_destroy_and_destroy_elements(packet, free);
          break;
        }
        int start_position = *(int*)list_get(packet, 0);
        char* bytes_to_write = (char*)list_get(packet, 1);
        int byte_count = *(int*)list_get(packet, 2);
        log_trace(ms.logger, "Write from Kernel Memory of %d bytes, from %d",
                  byte_count, start_position);
        write_memory(&ms, start_position, bytes_to_write, byte_count,
                     ms.socket_km);
        log_info(ms.logger, "Write of %d bytes", byte_count);
        list_destroy_and_destroy_elements(packet, free);
        break;
      }
      case OP_KERNEL_MEMORY_RUNNING:
        free(receive_string(ms.socket_km));
        break;
      default:
        log_warning(ms.logger,
                    "Unexpected operation %d from Kernel Memory; shutting down",
                    op_code);
        keep_running = false;
        break;
    }
  }

  log_info(ms.logger, "Memory Stick shutting down");
  close_module(&ms, &cpu_server_thread);
  return EXIT_SUCCESS;
}

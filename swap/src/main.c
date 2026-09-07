#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "swap/swap.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_swap swap = {0};

  if (argc != 2)
    return EXIT_FAILURE;
  char* config_path = argv[1];
  t_config* config = config_create(config_path);
  if (config == NULL)
    return EXIT_FAILURE;

  if (!init_config(&swap, config))
  {
    return EXIT_FAILURE;
  }

  if (!connect_to_kernel_memory(&swap, config))
  {
    return EXIT_FAILURE;
  }
  // Waiting for instructions from Kernel Memory.
  bool keep_running = true;
  while (keep_running)
  {
    int op_code = receive_op_code(swap.socket_swap);
    switch (op_code)
    {
      case OP_DISK_WRITE:
        t_list* packet = receive_packet(swap.socket_swap);
        if (list_size(packet) != 2)
        {
          log_error(swap.logger,
                    "Invalid number of parameters to write to disk.");
          break;
        }
        int block_number = *(int*)list_get(packet, 0);
        char* content = (char*)list_get(packet, 1);
        write_block(swap.swap_file, block_number, swap.block_size, content);
        send_string(OP_DISK_WRITE_DONE, "", swap.socket_swap);
        list_destroy_and_destroy_elements(packet, free);
        log_info(swap.logger, "## Block write: <%d>", block_number);
        break;

      case OP_DISK_READ:
        int a;
        int* block_number_ptr = (int*)receive_buffer(&a, swap.socket_swap);
        if (block_number_ptr == NULL)
        {
          log_error(swap.logger, "Error receiving the block number to read.");
          break;
        }
        char* content_read = malloc(swap.block_size);
        read_block(swap.swap_file, *block_number_ptr, swap.block_size,
                   content_read);
        send_buffer(OP_DISK_READ_DONE, content_read, swap.block_size,
                    swap.socket_swap);
        log_info(swap.logger, "## Block read: <%d>", *block_number_ptr);
        free(block_number_ptr);
        free(content_read);
        break;

      default:
        keep_running = false;
        break;
    }
  }
  log_info(swap.logger, "Closing Swap");
  close_swap(&swap, config);
  return EXIT_SUCCESS;
}

#include "swap/swap.h"

static bool init_swap_file(t_swap* swap);
static void seek_block(FILE* swap_file, int block_number, int block_size);

void close_swap(t_swap* swap, t_config* config)
{
  close(swap->socket_swap);
  fclose(swap->swap_file);
  log_destroy(swap->logger);
  config_destroy(config);
}

bool init_config(t_swap* swap, t_config* config)
{
  char* log_level_str = config_get_string_value(config, "LOG_LEVEL");

  swap->ip = config_get_string_value(config, "KERNEL_MEMORY_IP");
  swap->port = config_get_string_value(config, "KERNEL_MEMORY_PORT");
  swap->swap_size = config_get_int_value(config, "SWAP_FILE_SIZE");
  swap->block_size = config_get_int_value(config, "BLOCK_SIZE");
  swap->logger = log_create("swap.log", "SWAP", true,
                            log_level_from_string(log_level_str), false);
  if (swap->logger == NULL)
  {
    config_destroy(config);
    return false;
  }
  swap->swap_file_path = config_get_string_value(config, "SWAP_FILE_PATH");
  if (!init_swap_file(swap))
  {
    log_error(swap->logger, "## Error initializing the SWAP file");
    close_swap(swap, config);
    return false;
  }

  return true;
}

bool connect_to_kernel_memory(t_swap* swap, t_config* config)
{
  swap->socket_swap = create_connection(swap->ip, swap->port);

  if (swap->socket_swap == -1)
  {
    log_error(swap->logger, "#CONNECTION ERROR");
    close_swap(swap, config);
    return false;
  }
  log_info(swap->logger, "## Connected to Kernel Memory");

  bool sent_ok = send_handshake(MID_SWAP, swap->socket_swap);
  if (!sent_ok)
  {
    log_error(swap->logger, "## Handshake error with Kernel Memory");
    close_swap(swap, config);
    return false;
  }

  int received_id = receive_handshake(swap->socket_swap);
  if (received_id != MID_KERNEL_MEMORY)
  {
    log_error(swap->logger, "## Handshake error with Kernel Memory");
    close_swap(swap, config);
    return false;
  }
  log_info(swap->logger, "Handshake successful with Kernel Memory");

  // Send the swap size and block size to kernel memory.
  t_swap_config* km_config = malloc(sizeof(t_swap_config));
  int config_size = sizeof(t_swap_config);
  km_config->swap_size = swap->swap_size;
  km_config->block_size = swap->block_size;
  sent_ok = send_buffer(OP_INFO_SWAP, (void*)km_config, config_size,
                        swap->socket_swap);

  if (!sent_ok)
  {
    log_error(swap->logger, "## Error sending the SWAP packet");
    close_swap(swap, config);
    return false;
  }
  free(km_config);

  return true;
}

void write_block(FILE* swap_file, int block_number, int block_size,
                 char* content)
{
  seek_block(swap_file, block_number, block_size);
  fwrite(content, block_size, 1, swap_file);
  fflush(swap_file);
}

void read_block(FILE* swap_file, int block_number, int block_size,
                char* content)
{
  seek_block(swap_file, block_number, block_size);
  if (fread(content, block_size, 1, swap_file) != 1)
    return;
}

static bool init_swap_file(t_swap* swap)
{
  FILE* swap_file = fopen(swap->swap_file_path, "wb+");
  if (swap_file == NULL)
    return false;

  // Set the file size and fill it with zeros.
  int file_descriptor = fileno(swap_file);
  if (ftruncate(file_descriptor, swap->swap_size) == -1)
  {
    fclose(swap_file);
    return false;
  }

  swap->swap_file = swap_file;
  return true;
}

static void seek_block(FILE* swap_file, int block_number, int block_size)
{
  fseek(swap_file, block_number * block_size, 0);
}

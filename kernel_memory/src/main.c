#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/configurator.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/server.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  if (argc != 2)
    return EXIT_FAILURE;
  char* config_path = argv[1];

  t_config* config = init_config(config_path);
  t_log* logger = init_logger(config);
  int socket_kernel_memory =
      start_server(config_get_string_value(config, "KERNEL_MEMORY_PORT"));
  char* scripts_basepath = get_scripts_basepath(config);
  int instruction_delay = get_instruction_delay(config);
  int compaction_delay = get_compaction_delay(config);
  int segment_max_size = get_segment_max_size(config);
  int allocation_strategy = get_allocation_strategy(config);

  t_kernel_memory_data* kernel_data = init_kernel_memory_data(
      socket_kernel_memory, scripts_basepath, instruction_delay,
      compaction_delay, segment_max_size, allocation_strategy, logger);

  log_info(logger, "## Kernel Memory started");
  bool connection_alive = true;
  while (connection_alive)
  {
    connection_alive = accept_client(kernel_data);
  }

  free_kernel_memory_data(kernel_data);
  config_destroy(config);
  log_destroy(logger);
  return 0;
}

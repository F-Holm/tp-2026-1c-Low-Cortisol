#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_scheduler/connections/io.h"
#include "kernel_scheduler/connections/kernel_memory.h"
#include "kernel_scheduler/app/kernel_scheduler.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/connections/server.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"

int main(int argc, char* argv[])
{
  t_kernel_scheduler recursos = {0};

  // args
  if (argc != 3)
    return EXIT_FAILURE;
  char* config_path = argv[1];
  char* initial_process_path = argv[2];

  // Start the module
  if (!start_module(&recursos, config_path))
  {
    close_module_error(&recursos);
    return EXIT_FAILURE;
  }

  // Initialize data for the server
  init_scheduler_resources(&recursos);
  t_listen_server_data data;
  init_data_server_listen(&data, recursos.socket_server, recursos.logger,
                          recursos.mutex_list, recursos.queues,
                          recursos.km_socket_mutex, initial_process_path);

  // Start listening on the server
  server_listen(&data);

  // Release and close
  close_module(&recursos);
  return EXIT_SUCCESS;
}

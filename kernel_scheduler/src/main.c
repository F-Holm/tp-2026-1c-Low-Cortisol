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
  t_kernel_scheduler resources = {0};

  // args
  if (argc != 3)
    return EXIT_FAILURE;
  char* config_path = argv[1];
  char* initial_process_path = argv[2];

  // Start the module
  if (!start_module(&resources, config_path))
  {
    close_module_error(&resources);
    return EXIT_FAILURE;
  }

  // Initialize data for the server
  init_scheduler_resources(&resources);
  t_listen_server_data data;
  init_data_server_listen(&data, resources.socket_server, resources.logger,
                          resources.mutex_list, resources.queues,
                          resources.km_socket_mutex, initial_process_path);

  // Start listening on the server
  server_listen(&data);

  // Release and close
  close_module(&resources);
  return EXIT_SUCCESS;
}

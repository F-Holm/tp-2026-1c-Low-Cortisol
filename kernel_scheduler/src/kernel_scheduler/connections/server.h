#pragma once

#include "kernel_scheduler/syscalls/mutex.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "utils/log.h"

typedef struct
{
  int socket_server;
  t_log* logger;
  t_mutex_list* mutex_list;
  t_queues* queues;
  t_kernel_memory_socket* km_socket;
  char* initial_process_path;
} t_listen_server_data;

int create_socket_server(char* port, t_log* logger);
void init_data_server_listen(t_listen_server_data* data, int socket_server,
                             t_log* logger, t_mutex_list* mutex_list,
                             t_queues* queues,
                             t_kernel_memory_socket* socket_kernel_memory,
                             char* initial_process_path);
void server_listen(t_listen_server_data* data);

#pragma once

#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/syscalls/mutex.h"
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

/**
 * @brief Creates the listening server socket.
 * @return The socket fd, or -1 on failure.
 */
int create_socket_server(char* port, t_log* logger);

/** @brief Fills in @p data with the resources server_listen() needs. */
void init_data_server_listen(t_listen_server_data* data, int socket_server,
                             t_log* logger, t_mutex_list* mutex_list,
                             t_queues* queues,
                             t_kernel_memory_socket* socket_kernel_memory,
                             char* initial_process_path);

/**
 * @brief Runs the initial process, then accepts and dispatches CPU/IO
 *        connections until the socket is shut down.
 * @note Blocks until close_kernel_scheduler() shuts down the server socket.
 */
void server_listen(t_listen_server_data* data);

#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"
#include "utils/log.h"

typedef struct
{
  int server_socket;
  t_log* logger;
  t_kernel_memory_socket* km_socket;
  atomic_bool close;
  pthread_t thread;
} t_connection_check_thread;

int start_connection_kernel_memory(char* ip, char* port, t_log* logger);
bool notify_terminate_process(t_kernel_memory_socket* km_socket, uint32_t pid,
                              int server_socket, t_log* logger);
t_connection_check_thread* start_thread_check_connection_kernel_memory(
    int server_socket, t_log* logger, t_kernel_memory_socket* km_socket);
void destroy_thread_check_connection_kernel_memory(
    t_connection_check_thread* data);

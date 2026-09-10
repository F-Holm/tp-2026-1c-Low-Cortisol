#pragma once

#include <stdatomic.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"
#include "utils/log.h"

// Counts the processes currently alive in the system. When the count drops
// back to zero the scheduler shuts itself down (SR_NO_PROCESSES).
typedef struct
{
  atomic_int active_process_count;
  int server_socket;
  t_log* logger;
  t_kernel_memory_socket* km_socket;
} t_process_counter;

t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket);
void aumentar_counter_processes(t_process_counter* counter);
void disminuir_counter_processes(t_process_counter* counter);
void destroy_counter_processes(t_process_counter* counter);

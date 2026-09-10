#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"
#include "kernel_scheduler/domain/pcb.h"
#include "utils/collections/list.h"
#include "utils/log.h"

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_scheduling_algorithm;

typedef struct
{
  atomic_int active_process_count;
  int server_socket;
  t_log* logger;
  t_kernel_memory_socket* km_socket;
} t_process_counter;

typedef enum
{
  SR_NO_PROCESSES,
  SR_CORRUPTED_MEMORY,
  SR_KERNEL_MEMORY_CONNECTION_FAILURE,
  SR_UNKNOWN_CAUSE,
  SR_KERNEL_MEMORY_SEND_ERROR
} t_shutdown_reason;

extern const char* const SHUTDOWN_REASONS[4];

typedef enum
{
  SO_KM_ERROR,
  SO_IO_ERROR,
  SO_KM_CONNECTION_ERROR,
  SO_OK
} syscall_outcome;

void close_kernel_scheduler(int server_socket, t_log* logger,
                            int reason_shutdown, int km_socket);

bool respond_handshake(int socket_fd, int id_module, t_log* logger);
unsigned long millis(void);
unsigned long time_diff(unsigned long time_1, unsigned long time_2);
t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket);
void aumentar_counter_processes(t_process_counter* counter);
void disminuir_counter_processes(t_process_counter* counter);
void destroy_counter_processes(t_process_counter* counter);

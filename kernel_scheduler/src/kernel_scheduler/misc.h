#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "utils/collections/list.h"
#include "utils/log.h"

typedef enum
{
  EST_NEW,
  EST_READY,
  EST_EXEC,
  EST_BLOCK,
  EST_SUSP_BLOCK,
  EST_SUSP_READY,
  EST_EXIT
} t_process_state;

extern const char* const STATE_NAMES[7];

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_scheduling_algorithm;

typedef struct
{
  uint32_t pid;
  int priority;
  t_list* priority_list;
  pthread_mutex_t priority_mutex;
  unsigned long blocked_time;
  int state;
  pthread_mutex_t state_mutex;
  int active_instances;
  pthread_mutex_t active_instances_mutex;
  pthread_cond_t no_active_instances;
  void* blocking_mutex;
} t_pcb;

typedef struct
{
  int km_socket;
  pthread_mutex_t socket_mutex;
} t_kernel_memory_socket;

typedef struct
{
  int active_process_count;
  pthread_mutex_t counter_mutex;
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

void init_mutex_pid_pcb(void);
void init_mutex_shutdown(void);
void destroy_mutex_pid_pcb(void);
void destroy_mutex_shutdown(void);

void close_kernel_scheduler(int server_socket, t_log* logger,
                            int reason_shutdown, int km_socket);

t_kernel_memory_socket* init_socket_kernel_memory(int km_socket);
void destroy_kernel_memory(t_kernel_memory_socket* km_socket);
// returns the index of the inserted element
int insert_pcb_in_orden(t_list* list, t_pcb* pcb);
int get_state_pcb(t_pcb* pcb);
int get_priority_pcb(t_pcb* pcb);
t_pcb* create_pcb(int state, int priority);
void incrementar_instances_active_pcb(t_pcb* pcb);
void disminuir_instances_active_pcb(t_pcb* pcb);
void wait_0_instances_active_pcb(t_pcb* pcb);
void destroy_pcb(t_pcb* pcb);
void set_mutex_blocking(t_pcb* pcb, void* mutex);
void* get_mutex_blocking(t_pcb* pcb);
bool respond_handshake(int socket_fd, int id_module, t_log* logger);
unsigned long millis(void);
unsigned long time_diff(unsigned long time_1, unsigned long time_2);
t_process_counter* init_counter_processes(int server_socket, t_log* logger,
                                          t_kernel_memory_socket* km_socket);
void aumentar_counter_processes(t_process_counter* counter);
void disminuir_counter_processes(t_process_counter* counter);
void destroy_counter_processes(t_process_counter* counter);

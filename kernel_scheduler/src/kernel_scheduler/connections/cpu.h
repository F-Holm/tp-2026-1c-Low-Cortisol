#pragma once

#include <pthread.h>
#include <stdint.h>

#include "kernel_scheduler/connections/io.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "utils/collections/list.h"
#include "utils/log.h"

typedef struct
{
  int socket_fd;
  t_list* socket_list;
  pthread_mutex_t* socket_list_mutex;
  pthread_cond_t* done_cond;
  char* id;
  t_log* logger;
  t_mutex_list* mutex_list;
  t_queues* queues;
  t_io* io;
  t_kernel_memory_socket* km_socket;
  int server_socket;
} t_cpu_thread;

typedef struct
{
  t_cpu_thread* data;
  t_pcb* pcb;
  bool keep_running;
  unsigned long counter;
  int preemption_reason;
} t_syscall_data;

typedef enum
{
  PR_NO_PREEMPTION,
  PR_QUANTUM_END,
  PR_HIGHER_PRIORITY_PROCESS,
  PR_COMPACTION,
  PR_PROCESS_END,
  PR_FIRST_CYCLE,
  PR_IO,
  PR_MUTEX_LOCKED,
  PR_NOT_ENOUGH_MEMORY,
  PR_SEGMENTATION_FAULT,
  PR_MUTEX_NAME_ALREADY_EXISTS,
  PR_MUTEX_NAME_NOT_FOUND,
  PR_PROCESS_HAS_NO_LOCKED_MUTEX
} t_preemption_reason;

extern const char* const PREEMPTION_REASONS[13];

extern const char* const SYSCALL_NAMES[10];

bool handle_new_cpu(int socket_cpu, t_list* list_sockets_cpu,
                    pthread_mutex_t* mutex_list_sockets_cpu,
                    pthread_cond_t* cpu_done_cond, t_log* logger,
                    t_mutex_list* mutex_list, t_queues* queues, t_io* io,
                    t_kernel_memory_socket* km_socket, int server_socket);
void close_cpu(t_list* list_sockets_cpu,
               pthread_mutex_t* mutex_list_sockets_cpu,
               pthread_cond_t* cpu_done_cond, t_queues* queues);

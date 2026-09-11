#pragma once

#include <pthread.h>
#include <stdatomic.h>

#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/registers_cpu.h"

typedef enum
{
  BEST,
  WORST
} t_allocation_strategy;

typedef struct
{
  int compaction_delay;
  int total_size;
  int max_segment_size;
  t_list* segments;
  t_list* holes;
  int allocation_strategy;
  pthread_mutex_t* main_memory_mutex;
} t_main_memory;

typedef struct
{
  int socket_swap;
  t_log* logger;
  t_list* block_list;
  int swap_size;
  int block_size;
} t_swap_data;

typedef struct
{
  int socket_kernel_memory;
  int instruction_delay;
  int compaction_delay;
  int segment_max_size;
  t_allocation_strategy allocation_strategy;
  // Written by the accept-loop thread once the Kernel Scheduler connects,
  // read by the stick connection-check watchdog thread -- must be atomic.
  atomic_int socket_scheduler;
  char* scripts_basepath;
  t_log* logger;
  t_list* connected_sticks;
  t_list* connected_cpus;
  t_list* processes;
  t_main_memory* main_memory;
  t_swap_data* swap_data;
  pthread_mutex_t* processes_mutex;
  pthread_mutex_t* socket_list_mutex;
  int active_threads;
  pthread_mutex_t* active_threads_mutex;
  pthread_cond_t* active_threads_cond;
} t_kernel_memory_data;

typedef struct
{
  int socket_kernel_memory;
  int socket_scheduler;
  t_log* logger;
  t_list* processes;
  pthread_mutex_t* processes_mutex;
  char* scripts_basepath;
  t_list* connected_sticks;
  pthread_mutex_t* socket_list_mutex;
  t_main_memory* main_memory;
  t_swap_data* swap_data;
  int* active_threads;
  pthread_mutex_t* active_threads_mutex;
  pthread_cond_t* active_threads_cond;
} t_scheduler_data;

typedef struct
{
  int id;
  int socket_cpu;
  int socket_scheduler;
  t_log* logger;
  int instruction_delay;
  t_list* processes;
  pthread_mutex_t* processes_mutex;
  t_main_memory* main_memory;
  int* active_threads;
  pthread_mutex_t* active_threads_mutex;
  pthread_cond_t* active_threads_cond;
} t_cpu_data;

typedef struct
{
  int stick_size;
  int socket_stick;
  char ip_memory_stick[16];
  int stick_port;
  int socket_scheduler;
  t_log* logger;
} t_stick_data;

typedef struct
{
  uint32_t pid;
  char* instructions_path;
  char** instructions;
  int instruction_count;
  t_list* segments;
  t_registers registers;
} t_process;

typedef struct
{
  int base;
  int size;
} t_hole;

typedef struct
{
  int block_number;
  uint32_t pid;
  int segment_number;
  int segment_block_number;
  int segment_size;
} t_block_data;

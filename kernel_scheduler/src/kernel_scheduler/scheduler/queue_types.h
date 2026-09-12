#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/domain/kernel_memory_socket.h"
#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/process_counter.h"
#include "utils/collections/list.h"
#include "utils/log.h"

typedef enum
{
  AP_FIFO,
  AP_RR,
  AP_CMN
} t_scheduling_algorithm;

typedef enum
{
  PER_INVALID_PRIORITY,
  PER_EXIT_INSTRUCTION,
  PER_SYSTEM_SHUTDOWN,
  PER_IO_FAILURE,
  PER_NOT_ENOUGH_MEMORY,
  PER_SEGMENTATION_FAULT,
  PER_MUTEX_NAME_ALREADY_EXISTS,
  PER_MUTEX_NAME_NOT_FOUND,
  PER_PROCESS_HAS_NO_LOCKED_MUTEX
} t_process_end_reason;

extern const char* const PROCESS_END_REASONS[9];

// A count with a condition variable, used to wait for worker threads to drain
// and to gate on "the exec queue holds nothing but in-flight syscalls".
typedef struct
{
  int count;
  pthread_mutex_t counter_mutex;
  pthread_cond_t condition;
} t_counter;

// BLOCK, SUSP. BLOCK and SUSP. READY are each a plain list plus a
// new-arrival signal for the suspender/resumer threads that watch them.
typedef struct
{
  t_list* list;
  pthread_mutex_t list_mutex;
  pthread_cond_t new_process_cond;
  bool new_process;
} t_blocking_list;

typedef struct
{
  t_list* queue;
  int algorithm;
} t_ready_subqueue;

typedef struct
{
  int queue_count;
  t_ready_subqueue* queues;
  bool multilevel_queue;
  pthread_mutex_t queue_mutex;
  int ready_process_count;
  pthread_cond_t new_process;
  pthread_cond_t exit_unblocked;
  atomic_bool terminate_queue;
  atomic_bool preempt_all;
  int highest_priority;
  pthread_cond_t queue_empty;
} t_ready_queue;

typedef struct
{
  t_list* list;
  pthread_mutex_t list_mutex;
  pthread_cond_t queue_empty;
  int lowest_priority;
  int quantum;      // = 0 if not RR
  bool preemption;  // whether preemption is enabled
} t_execute_list;   // since some values never change (quantum and
                    // preemption), they do not need a mutex

typedef enum
{
  HS_RUNNING,
  HS_WAITING_PROCESS,
  HS_BLOCKED,
  HS_FINISHING,
  HS_FINISHED
} t_thread_state;

typedef struct
{
  pthread_t thread;
  pthread_mutex_t state_mutex;
  int state;
  pthread_cond_t* wait_process;
  pthread_cond_t unlock;
} t_suspended_thread;

typedef struct
{
  t_suspended_thread* data;
  int suspension_timeout;
} t_suspender_thread;

typedef struct
{
  t_suspended_thread* data;
} t_resumer_thread;

typedef struct
{
  t_suspender_thread* suspender_thread_data;
  t_resumer_thread* resumer_thread_data;
} t_suspension_data;

// The whole scheduler state. Every queue lives here by value; the worker
// threads, counters and routine flags hang off it too.
typedef struct
{
  t_ready_queue ready;
  t_execute_list exec;
  t_blocking_list block;
  t_blocking_list susp_block;
  t_blocking_list susp_ready;
  t_process_counter* process_counter;
  t_counter* thread_counter;
  t_counter* syscall_counter;
  t_log* logger;
  t_kernel_memory_socket* km_socket;
  int server_socket;
  t_suspension_data* suspension_data;
  pthread_mutex_t routine_mutex;
  pthread_cond_t routine_cond;
  bool routine_active;
  bool terminate_routines;
  atomic_bool compaction_active;
  atomic_bool resume_active;
} t_queues;

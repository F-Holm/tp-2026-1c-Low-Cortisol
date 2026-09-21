#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/process_counter.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"
#include "utils/threads.h"

typedef enum
{
  SA_FIFO,
  SA_RR,
  SA_MULTILEVEL_QUEUES  // "MULTILEVEL" in the config file
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
  mtx_t counter_mutex;
  cnd_t condition;
} t_counter;

// BLOCK, SUSP. BLOCK and SUSP. READY are each a plain list plus a
// new-arrival signal for the suspender/resumer threads that watch them.
typedef struct
{
  t_list* list;
  mtx_t list_mutex;
  cnd_t new_process_cond;
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
  mtx_t queue_mutex;
  int ready_process_count;
  cnd_t new_process;
  cnd_t exit_unblocked;
  atomic_bool terminate_queue;
  atomic_bool preempt_all;
  int highest_priority;
  cnd_t queue_empty;
} t_ready_queue;

typedef struct
{
  t_list* list;
  mtx_t list_mutex;
  cnd_t queue_empty;
  int lowest_priority;
  int quantum;      // = 0 if not RR
  bool preemption;  // whether preemption is enabled
} t_execute_list;   // since some values never change (quantum and
                    // preemption), they do not need a mutex

typedef enum
{
  TS_RUNNING,
  TS_WAITING_PROCESS,
  TS_BLOCKED,
  TS_FINISHING,
  TS_FINISHED
} t_thread_state;

typedef struct
{
  thrd_t thread;
  mtx_t state_mutex;
  int state;
  cnd_t* wait_process;
  cnd_t unlock;
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

// Everything that coordinates the compaction/resumption background
// routines: the counters they wait on, the mutex/condition/flags that gate
// a routine running at a time, and the suspender/resumer thread handles.
typedef struct
{
  t_counter* thread_counter;
  t_counter* syscall_counter;
  mtx_t routine_mutex;
  cnd_t routine_cond;
  bool routine_active;
  bool terminate_routines;
  atomic_bool compaction_active;
  atomic_bool resume_active;
  t_suspension_data* suspension_data;
} t_routine_state;

// The whole scheduler state. Every queue lives here by value; the shared
// resources and the routine-coordination state hang off it too.
typedef struct
{
  t_ready_queue ready;
  t_execute_list exec;
  t_blocking_list block;
  t_blocking_list susp_block;
  t_blocking_list susp_ready;
  t_process_counter* process_counter;
  t_log* logger;
  t_socket* km_socket;
  t_routine_state routines;
} t_queues;

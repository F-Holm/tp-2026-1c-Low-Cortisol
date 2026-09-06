#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/kernel_memory.h"
#include "kernel_scheduler/misc.h"
#include "utils/collections/list.h"
#include "utils/log.h"

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

typedef struct
{
  int count;
  pthread_mutex_t counter_mutex;
  pthread_cond_t condition;
} t_counter;

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
  pthread_mutex_t block_exit;
  bool terminate_queue;
  pthread_mutex_t terminate_queue_mutex;
  bool preempt_all;
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
  bool terminate_routines;
  pthread_mutex_t compaction_active_mutex;
  bool compaction_active;
  pthread_mutex_t resume_active_mutex;
  bool resume_active;
} t_queues;

// pass NULL as t_list if the algorithm is not CMN
// pass quantum = 0 if the algorithm is not RR
t_queues* init_queues(int algorithm, t_list* cmn_algorithms, int quantum,
                      bool preemption, int server_socket, t_log* logger,
                      t_kernel_memory_socket* km_socket,
                      int suspension_timeout);
void destroy_queues(t_queues* queues);

bool is_queue_ready_empty(t_ready_queue* ready);
bool is_queue_ready_blocked(t_ready_queue* ready);
void lock_queue_ready(t_ready_queue* ready);
void unlock_queue_ready(t_ready_queue* ready);
bool queue_ready_terminated(t_ready_queue* ready);
void terminate_queue_ready(t_ready_queue* ready);

void update_lowest_exec_priority(t_execute_list* exec);
void wait_queue_exec_empty(t_queues* queues);
void wait_queue_exec_empty_with_syscalls(t_queues* queues);

bool can_suspend(t_pcb* pcb, int suspension_timeout);
void update_priority(t_pcb* pcb, t_queues* queues);

// transition_ready_exec: not implemented, only logs for now. Use the
// individual functions
void transition_to_exec(t_pcb* pcb, t_execute_list* exec);
t_pcb* transition_take_ready_blocking(t_ready_queue* ready);
t_pcb* transition_take_ready_next_no_mutex(t_ready_queue* ready);
void transition_ready_exec(t_pcb* pcb, t_queues* queues);

// state-change functions
void transition_new_ready(t_queues* queues, char* instructions_file,
                          int priority);
void transition_exec_ready(t_pcb* pcb, t_queues* queues);
void transition_exec_exit(t_pcb* pcb, t_queues* queues, int reason);
void transition_exec_block(t_pcb* pcb, t_queues* queues);
void transition_block_ready(t_pcb* pcb, t_queues* queues);
void transition_block_susp_block(t_pcb* pcb, t_queues* queues);
void transition_susp_block(t_pcb* pcb, t_queues* queues);
void transition_susp_block_susp_ready(t_pcb* pcb, t_queues* queues);
bool transition_susp_ready(t_pcb* pcb, t_queues* queues);
void transition_unlock(t_pcb* pcb, t_queues* queues);

// for errors or shutdown routines
void clear_queues(t_queues* queues);

// to lock and unlock the suspender and resumer threads
void lock_threads_suspended(t_queues* queues);
void unlock_threads_suspended(t_queues* queues);

// kernel_memory query functions
int space_available_no_mutex(t_queues* queues, uint32_t pid);
int space_available(t_queues* queues, uint32_t pid);
int process_size_no_mutex(t_queues* queues, uint32_t pid);
int process_size(t_queues* queues, uint32_t pid);

// routine functions
void create_resumption_routine_thread(t_queues* queues);
void routine_compaction(t_queues* queues);
bool is_compacting(t_queues* queues);
bool is_resuming(t_queues* queues);

// Counters
void increment_syscall_counter(t_queues* queues);
void decrement_syscall_counter(t_queues* queues);

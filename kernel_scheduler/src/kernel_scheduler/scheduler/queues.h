#pragma once

#include <stdint.h>

#include "kernel_scheduler/scheduler/blocking_list.h"
#include "kernel_scheduler/scheduler/counter.h"
#include "kernel_scheduler/scheduler/exec_list.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "kernel_scheduler/scheduler/ready_queue.h"

// pass NULL as t_list if the algorithm is not CMN
// pass quantum = 0 if the algorithm is not RR
t_queues* init_queues(int algorithm, t_list* cmn_algorithms, int quantum,
                      bool preemption, int server_socket, t_log* logger,
                      t_kernel_memory_socket* km_socket,
                      int suspension_timeout);
void destroy_queues(t_queues* queues);

bool can_suspend(t_pcb* pcb, int suspension_timeout);
void update_priority(t_pcb* pcb, t_queues* queues);

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

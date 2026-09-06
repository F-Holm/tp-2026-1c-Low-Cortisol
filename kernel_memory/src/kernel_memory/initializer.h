#pragma once

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/swap_km.h"

t_kernel_memory_data* init_kernel_memory_data(
    int socket_kernel_memory, char* scripts_basepath, int instruction_delay,
    int compaction_delay, int segment_max_size,
    t_allocation_strategy allocation_strategy, t_log* logger);
t_scheduler_data* init_scheduler_data(
    int socket_kernel_memory, int socket_scheduler, t_list* processes,
    char* scripts_basepath, pthread_mutex_t* processes_mutex,
    t_main_memory* main_memory, t_list* connected_sticks,
    pthread_mutex_t* sticks_mutex, t_swap_data* swap_data, t_log* logger,
    int* active_threads, pthread_mutex_t* active_threads_mutex,
    pthread_cond_t* active_threads_cond);
t_cpu_data* init_cpu_data(int socket_cpu, t_list* processes,
                          pthread_mutex_t* processes_mutex,
                          int instruction_delay, t_main_memory* main_memory,
                          t_log* logger, int* active_threads,
                          pthread_mutex_t* active_threads_mutex,
                          pthread_cond_t* active_threads_cond,
                          int socket_scheduler);
t_stick_data* init_stick_data(int socket_stick, t_log* logger,
                              int socket_scheduler);
t_swap_data* init_swap_data(int socket_swap, t_log* logger);
t_process* init_process(u_int32_t pid, char* path_relativo,
                        char* scripts_basepath, t_log* logger);
bool resolve_stick_ip(t_stick_data* stick_data, int client_socket);
t_main_memory* init_main_memory(int total_size,
                                t_allocation_strategy allocation_strategy,
                                int compaction_delay);

#pragma once

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "kernel_scheduler/connections/kernel_memory.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registers_cpu.h"
#include "utils/syscalls.h"

extern const char* const SCHEDULING_ALGORITHMS[3];

typedef struct
{
  t_log_level log_level;
  int scheduling_algorithm;
  t_list* cmn_algorithms;
  int rr_quantum;
  bool preemption;
  int suspension_timeout;
  char* server_port;
  char* kernel_memory_ip;
  char* kernel_memory_port;
} t_config_vars;

typedef struct
{
  int socket_kernel_memory;
  int socket_server;
  t_config* config;
  t_log* logger;
  t_config_vars config_vars;
  t_mutex_list* mutex_list;
  t_queues* queues;
  t_kernel_memory_socket* km_socket_mutex;
  t_connection_check_thread* connection_check_thread_data;
} t_kernel_scheduler;

bool start_module(t_kernel_scheduler* resources, char* config_path);
void init_scheduler_resources(t_kernel_scheduler* resources);
void close_module_error(t_kernel_scheduler* resources);
void close_module(t_kernel_scheduler* resources);

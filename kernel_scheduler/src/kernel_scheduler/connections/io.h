#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "kernel_scheduler/app/kernel_scheduler.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/string.h"

/****************** IO FUNCTIONS ******************/

typedef struct
{
  t_list* io_list;
  pthread_mutex_t io_list_mutex;
} t_io_list;
typedef struct
{
  int socket_io;
  t_pcb* current_process;
  bool priority_active;
  pthread_cond_t new_process;
  t_queues* queues;
  t_log* logger;
  t_kernel_memory_socket* km_socket;
  int socket_server;
  atomic_bool close_thread;
  pthread_t io_thread;
  t_io_list* io_list;
  int io_type;
} t_io;

typedef struct
{
  t_pcb* pcb;
  t_stdin_request* request;
} t_stdin;

typedef struct
{
  t_pcb* pcb;
  t_stdout_request* request;
} t_stdout;

typedef struct
{
  t_pcb* pcb;
  t_sleep_request* request;
} t_sleep;

t_io* create_estructuras_io(void);
bool handle_new_io(t_io io[3], int socket_fd, t_queues* queues,
                   bool priority_active, int socket_server);
bool procesar_new_io(void* request, t_io* io, t_pcb* pcb);
void close_io(t_io* io);

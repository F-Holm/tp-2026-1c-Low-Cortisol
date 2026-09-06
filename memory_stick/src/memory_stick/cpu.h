#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "memory_stick/memory_stick.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"

typedef struct
{
  int cpu_listen_socket;
  t_log* logger;
  t_ms_recursos* ms;
} t_listen_thread;

typedef struct
{
  int socket_cpu;
  t_list* socket_list;
  pthread_mutex_t* socket_list_mutex;
  pthread_cond_t* listen_done_cond;
  t_ms_recursos* ms;
} t_cpu_thread;

int create_server_cpu(t_log* logger);
uint16_t get_cpu_port(int socket_server_cpu);
void iterator_shutdown(void* value);
t_cpu_thread* create_cpu_thread_data(int socket_cpu, t_list* socket_list,
                                     pthread_mutex_t* socket_list_mutex,
                                     pthread_cond_t* listen_done_cond,
                                     t_ms_recursos* ms);
bool spawn_cpu_thread(t_cpu_thread* cpu_thread, t_log* logger);
void close_listen_thread(t_list* socket_list, pthread_mutex_t* socket_list_mutex,
                         pthread_cond_t* listen_done_cond,
                         t_listen_thread* listen_thread);
bool handshake_cpu(int socket_cpu, t_log* logger);
char* receive_cpu_id(int socket_cpu, t_log* logger);
bool handle_new_cpu(t_listen_thread* listen_thread, int socket_cpu,
                    t_list* socket_list, pthread_mutex_t* socket_list_mutex,
                    pthread_cond_t* listen_done_cond, t_ms_recursos* ms);
void* cpu_listen_thread(void* listen_thread_void);
void* handle_cpu_client(void* cpu_thread_void);
void close_cpu_thread(t_cpu_thread* cpu_thread);
bool start_cpu_server(pthread_t* cpu_server_thread, int cpu_server_socket,
                      t_log* logger, t_ms_recursos* ms);

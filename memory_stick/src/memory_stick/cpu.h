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
  t_ms* ms;
} t_listen_thread;

typedef struct
{
  int socket_cpu;
  t_list* socket_list;
  pthread_mutex_t* socket_list_mutex;
  pthread_cond_t* listen_done_cond;
  t_ms* ms;
} t_cpu_thread;

/**
 * @brief Creates the listening TCP socket CPUs connect to.
 * @return The listening socket fd, or -1 on failure.
 */
int create_server_cpu(t_log* logger);

/** @brief Returns the port the CPU server socket is bound to. */
uint16_t get_cpu_port(int socket_server_cpu);

/** @brief list_iterate() closure: shuts down the socket fd at @p value. */
void iterator_shutdown(void* value);

/**
 * @brief Allocates the per-connection state passed to handle_cpu_client().
 * @note Freed by close_cpu_thread(); free it directly if the thread is
 *       never spawned.
 */
t_cpu_thread* create_cpu_thread_data(int socket_cpu, t_list* socket_list,
                                     pthread_mutex_t* socket_list_mutex,
                                     pthread_cond_t* listen_done_cond,
                                     t_ms* ms);

/**
 * @brief Spawns a detached thread running handle_cpu_client() for
 *        @p cpu_thread.
 * @return false if the thread could not be created.
 */
bool spawn_cpu_thread(t_cpu_thread* cpu_thread, t_log* logger);

/**
 * @brief Shuts down every registered CPU socket and frees the listen-thread
 *        state.
 * @note Blocks until every CPU client thread has exited.
 */
void close_listen_thread(t_list* socket_list,
                         pthread_mutex_t* socket_list_mutex,
                         pthread_cond_t* listen_done_cond,
                         t_listen_thread* listen_thread);

/**
 * @brief Performs the handshake with a newly connected CPU.
 * @return false if the handshake failed.
 */
bool handshake_cpu(int socket_cpu, t_log* logger);

/**
 * @brief Receives the connecting CPU's id string.
 * @return The id (caller must free), or NULL on error.
 */
char* receive_cpu_id(int socket_cpu, t_log* logger);

/**
 * @brief Handshakes a new CPU connection, registers its socket and spawns
 *        its handling thread.
 * @return false if any step failed; the caller should close the socket.
 */
bool handle_new_cpu(t_listen_thread* listen_thread, int socket_cpu,
                    t_list* socket_list, pthread_mutex_t* socket_list_mutex,
                    pthread_cond_t* listen_done_cond, t_ms* ms);

/**
 * @brief Thread entry point: accepts CPU connections until the server
 *        socket is closed.
 * @param listen_thread_void A t_listen_thread*, freed internally.
 */
void* cpu_listen_thread(void* listen_thread_void);

/**
 * @brief Thread entry point: serves read/write requests from one CPU until
 *        it disconnects.
 * @param cpu_thread_void A t_cpu_thread*, freed internally.
 */
void* handle_cpu_client(void* cpu_thread_void);

/**
 * @brief Closes the CPU connection, removes it from the socket list and
 *        frees @p cpu_thread.
 */
void close_cpu_thread(t_cpu_thread* cpu_thread);

/**
 * @brief Starts the CPU server thread (cpu_listen_thread()).
 * @return false if the thread could not be created.
 */
bool start_cpu_server(pthread_t* cpu_server_thread, int cpu_server_socket,
                      t_log* logger, t_ms* ms);

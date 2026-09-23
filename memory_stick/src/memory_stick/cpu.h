#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <threads.h>

#include "memory_stick/memory_stick.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/sockets.h"

typedef struct
{
  t_socket* cpu_listen_socket;
  t_log* logger;
  t_ms* ms;
} t_listen_thread;

typedef struct
{
  t_socket* socket_cpu;
  t_list* socket_list;
  mtx_t* socket_list_mutex;
  cnd_t* listen_done_cond;
  t_ms* ms;
} t_cpu_thread;

/**
 * @brief Creates the listening TCP socket CPUs connect to.
 * @return The listening socket, or NULL on failure.
 */
t_socket* create_server_cpu(t_log* logger);

/** @brief Returns the port the CPU server socket is bound to. */
uint16_t get_cpu_port(t_socket* socket_server_cpu);

/** @brief list_iterate() closure: shuts down the t_socket* at @p value. */
void iterator_shutdown(void* value);

/**
 * @brief Allocates the per-connection state passed to handle_cpu_client().
 * @note Freed by close_cpu_thread(); free it directly if the thread is
 *       never spawned.
 */
t_cpu_thread* create_cpu_thread_data(t_socket* socket_cpu, t_list* socket_list,
                                     mtx_t* socket_list_mutex,
                                     cnd_t* listen_done_cond, t_ms* ms);

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
void close_listen_thread(t_list* socket_list, mtx_t* socket_list_mutex,
                         cnd_t* listen_done_cond,
                         t_listen_thread* listen_thread);

/**
 * @brief Performs the handshake with a newly connected CPU.
 * @return false if the handshake failed.
 */
bool handshake_cpu(t_socket* socket_cpu, t_log* logger);

/**
 * @brief Receives the connecting CPU's id string.
 * @return The id (caller must free), or NULL on error.
 */
char* receive_cpu_id(t_socket* socket_cpu, t_log* logger);

/**
 * @brief Handshakes a new CPU connection, registers its socket and spawns
 *        its handling thread.
 * @return false if any step failed; the caller should close the socket.
 */
bool handle_new_cpu(t_listen_thread* listen_thread, t_socket* socket_cpu,
                    t_list* socket_list, mtx_t* socket_list_mutex,
                    cnd_t* listen_done_cond, t_ms* ms);

/**
 * @brief Thread entry point: accepts CPU connections until the server
 *        socket is closed.
 * @param listen_thread_void A t_listen_thread*, freed internally.
 */
int cpu_listen_thread(void* listen_thread_void);

/**
 * @brief Thread entry point: serves read/write requests from one CPU until
 *        it disconnects.
 * @param cpu_thread_void A t_cpu_thread*, freed internally.
 */
int handle_cpu_client(void* cpu_thread_void);

/**
 * @brief Closes the CPU connection, removes it from the socket list and
 *        frees @p cpu_thread.
 */
void close_cpu_thread(t_cpu_thread* cpu_thread);

/**
 * @brief Starts the CPU server thread (cpu_listen_thread()).
 * @return false if the thread could not be created.
 */
bool start_cpu_server(thrd_t* cpu_server_thread, t_socket* cpu_server_socket,
                      t_log* logger, t_ms* ms);

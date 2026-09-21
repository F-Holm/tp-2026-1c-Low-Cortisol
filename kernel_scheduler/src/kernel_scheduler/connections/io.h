#pragma once

#include <stdatomic.h>
#include <stdbool.h>

#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queue_types.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"
#include "utils/syscalls.h"
#include "utils/threads.h"

/****************** IO FUNCTIONS ******************/

typedef struct
{
  t_list* io_list;
  mtx_t io_list_mutex;
} t_io_list;
typedef struct
{
  t_socket* socket_io;
  t_pcb* current_process;
  bool priority_active;
  cnd_t new_process;
  t_queues* queues;
  t_log* logger;
  t_socket* km_socket;
  atomic_bool close_thread;
  thrd_t io_thread;
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

/** @brief Allocates the 3-element STDIN/STDOUT/SLEEP t_io array. */
t_io* create_io_structures(void);

/**
 * @brief Handshakes a freshly accepted IO connection, fills in its slot in
 *        @p io (by type) and spawns its serving thread.
 * @return false on handshake failure or a duplicate/invalid IO type.
 */
bool handle_new_io(t_io io[3], t_socket* socket_fd, t_queues* queues,
                   bool priority_active);

/**
 * @brief Queues @p request for @p pcb on @p io, waking its thread.
 * @return false if the IO's thread is already shutting down.
 */
bool enqueue_io_request(void* request, t_io* io, t_pcb* pcb);

/** @brief Stops and joins every IO thread, discarding pending requests, and
 *         frees @p io. */
void close_io(t_io* io);

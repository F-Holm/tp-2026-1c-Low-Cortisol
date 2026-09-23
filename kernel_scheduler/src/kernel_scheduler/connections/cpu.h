#pragma once

#include <stdbool.h>
#include <threads.h>

#include "kernel_scheduler/connections/io.h"
#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/queues.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/sockets.h"

typedef struct
{
  t_socket* socket_fd;
  t_list* socket_list;
  mtx_t* socket_list_mutex;
  cnd_t* done_cond;
  char* id;
  t_log* logger;
  t_mutex_list* mutex_list;
  t_queues* queues;
  t_io* io;
  t_socket* km_socket;
  // per-cycle state, reset by init_data_thread_cpu, mutated by
  // handle_cpu_client's own thread only
  t_pcb* pcb;
  bool keep_running;
  unsigned long counter;
  int preemption_reason;
} t_cpu_thread;

typedef enum
{
  PR_NO_PREEMPTION,
  PR_QUANTUM_END,
  PR_HIGHER_PRIORITY_PROCESS,
  PR_COMPACTION,
  PR_PROCESS_END,
  PR_FIRST_CYCLE,
  PR_IO,
  PR_MUTEX_LOCKED,
  PR_NOT_ENOUGH_MEMORY,
  PR_SEGMENTATION_FAULT,
  PR_MUTEX_NAME_ALREADY_EXISTS,
  PR_MUTEX_NAME_NOT_FOUND,
  PR_PROCESS_HAS_NO_LOCKED_MUTEX
} t_preemption_reason;

extern const char* const PREEMPTION_REASONS[13];

extern const char* const SYSCALL_NAMES[10];

/**
 * @brief Handshakes a freshly accepted CPU connection, registers its socket
 *        and spawns its serving thread.
 * @return false on handshake/registration failure (caller should close the
 *         socket).
 */
bool handle_new_cpu(t_socket* socket_cpu, t_list* list_sockets_cpu,
                    mtx_t* mutex_list_sockets_cpu, cnd_t* cpu_done_cond,
                    t_log* logger, t_mutex_list* mutex_list, t_queues* queues,
                    t_io* io, t_socket* km_socket);

/** @brief Terminates the ready queue and waits for every CPU thread to
 *         finish before returning. */
void close_cpu(t_list* list_sockets_cpu, mtx_t* mutex_list_sockets_cpu,
               cnd_t* cpu_done_cond, t_queues* queues);

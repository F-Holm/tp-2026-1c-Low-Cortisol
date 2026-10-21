#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#include "utils/collections/list.h"
#include "utils/sockets.h"

/**
 * @file
 * @brief Generic connection-check (watchdog) thread: periodically pings every
 *        socket in a list to detect a dropped connection. Generalizes the
 *        pattern duplicated today by kernel_memory's stick watchdog and
 *        kernel_scheduler's Kernel Memory connection-check thread.
 *
 * Each ping locks the target t_socket's own mutex for its duration (see
 * socket_mutex_lock/unlock in sockets.h), so a ping never races a real
 * read/write some other thread is doing over that same socket -- every
 * socket in the list must simply have been created with a mutex
 * (socket_create(..., with_mutex=true)). The list itself is a separate
 * concern: if some other thread can add/remove sockets to/from it while the
 * watchdog is running, pass a mutex guarding it; otherwise pass NULL.
 */

/** @brief A connection-check thread watching every socket in @p sockets. */
typedef struct
{
  t_list* sockets;  // elements to periodically ping -- t_socket* themselves
                    // if get_socket is NULL, or whatever get_socket expects
                    // otherwise; each socket must have been created with
                    // with_mutex=true
  t_socket* (*get_socket)(void* element);  // extracts the t_socket* to ping
                                           // from a @p sockets element; NULL
                                           // if elements already are
                                           // t_socket*
  mtx_t* sockets_mutex;  // optional mutex guarding @p sockets itself (e.g. if
                         // another thread adds/removes elements concurrently);
                         // NULL if no such mutex is needed

  unsigned long cycle_interval_ms;  // sleep between finishing one full pass
                                    // over @p sockets and starting the next

  void (*on_disconnected)(void* arg);  // called when a ping fails
  void* callback_arg;                  // passed to on_disconnected as-is

  thrd_t thread;
  atomic_bool close;
} t_watchdog;

/**
 * @brief Starts the watchdog thread: repeatedly walks @p sockets, pinging
 *        each one (through @p get_socket if not NULL), then sleeps
 *        @p cycle_interval_ms before the next pass. Calls @p on_disconnected
 *        (with @p callback_arg) for every ping that fails. @p sockets_mutex
 *        is locked/unlocked around each pass if not NULL.
 */
t_watchdog* watchdog_start(t_list* sockets,
                           t_socket* (*get_socket)(void* element),
                           mtx_t* sockets_mutex,
                           unsigned long cycle_interval_ms,
                           void (*on_disconnected)(void* arg),
                           void* callback_arg);

/**
 * @brief Stops the watchdog thread and frees @p watchdog. Does not free
 *        @p sockets, its elements, or @p callback_arg.
 */
void watchdog_destroy(t_watchdog* watchdog);

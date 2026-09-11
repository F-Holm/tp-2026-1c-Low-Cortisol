#pragma once

#include "kernel_scheduler/scheduler/queues.h"

/**
 * @file
 * @brief Shared helpers for the kernel_scheduler module test suites.
 */

/** @brief A console-only logger that stays silent below ERROR. */
t_log* ks_quiet_logger(void);

/**
 * @brief A zeroed `t_queues` carrying only a logger -- enough for the mutex
 *        paths that just log. Free it with `free()`.
 */
t_queues* ks_stub_queues(t_log* logger);

/**
 * @brief Like `ks_stub_queues()`, but with real (empty) `exec`/`block` lists
 *        and a real `syscall_counter` -- enough for `mutex_lock`'s blocking
 *        path, which moves a process from EXEC to BLOCK. Destroy it with
 *        `ks_destroy_stub_queues_blocking()`, not a plain `free()`.
 */
t_queues* ks_stub_queues_blocking(t_log* logger);
void ks_destroy_stub_queues_blocking(t_queues* queues);

/**
 * @brief A `t_queues` with every in-process queue really initialised (a single
 *        FIFO ready queue, real exec/block/susp lists, real counters and a
 *        process counter wired to a dead socket fd). Enough for the
 *        socket-free state transitions (EXEC<->READY, EXEC->BLOCK,
 *        BLOCK->READY, ...). The suspender/resumer threads are NOT started.
 *        Destroy it with `ks_destroy_stub_queues_full()`.
 */
t_queues* ks_stub_queues_full(t_log* logger);
void ks_destroy_stub_queues_full(t_queues* queues);

/**
 * @brief A loopback TCP listener on a kernel-assigned ephemeral port, for
 *        tests that need a real socket without a fixed port. Writes the port
 *        number (as a string) into `port_out`.
 */
int ks_listen_ephemeral(char* port_out, int port_len);

/**
 * @brief A connected loopback TCP pair: connects to a fresh ephemeral
 *        listener and accepts that same connection, so both ends are real
 *        sockets. Returns the client fd; writes the server-side fd into
 *        `server_out`.
 */
int ks_connected_pair(int* server_out);

/**
 * @brief Blocks until queues->thread_counter drops to 0 -- the same
 *        synchronization destroy_queues() uses to wait out any detached
 *        worker thread (e.g. one spawned by create_resumption_routine_thread)
 *        before tearing queues down. Call this before destroying a
 *        ks_stub_queues_full() whenever a test may have triggered one.
 */
void ks_wait_thread_counter_zero(t_queues* queues);

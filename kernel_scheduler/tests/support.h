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

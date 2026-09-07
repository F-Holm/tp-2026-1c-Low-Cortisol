#pragma once

#include "kernel_scheduler/misc.h"
#include "kernel_scheduler/queue.h"

/**
 * @file
 * @brief Shared helpers for the kernel_scheduler module test suites.
 */

/**
 * @brief Initialises the module-global mutexes that `create_pcb()` and the
 *        shutdown path rely on. Call once at the start of every test that
 *        builds a PCB.
 */
void ks_init_globals(void);

/** @brief A console-only logger that stays silent below ERROR. */
t_log* ks_quiet_logger(void);

/**
 * @brief A zeroed `t_queues` carrying only a logger -- enough for the mutex
 *        paths that just log. Free it with `free()`.
 */
t_queues* ks_stub_queues(t_log* logger);

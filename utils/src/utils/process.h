#pragma once

/**
 * @file
 * @brief Platform-agnostic identifiers of the running process and thread.
 *
 * Backed today by process_linux.c. A future Windows backend would provide the
 * same functions in process_windows.c (guarded by #ifdef OS_WINDOWS).
 */

/** @brief The OS identifier of the current process. */
int process_get_id(void);

/**
 * @brief The OS identifier of the calling thread.
 * @note Unique across the whole system, unlike a thrd_t, and the same number
 *       tools such as `top -H` show.
 */
long process_get_thread_id(void);

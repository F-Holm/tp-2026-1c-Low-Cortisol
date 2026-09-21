#pragma once

#include "utils/threads_linux.h"
// -- future Windows support: switch the include above between
//    "utils/threads_linux.h" and a new "utils/threads_windows.h" behind an
//    #ifdef _WIN32; both must define thrd_t/thrd_start_t. Not implemented
//    yet.

/**
 * @file
 * @brief Mirrors the names of C11 <threads.h>'s thread subset (thrd_t,
 *        thrd_create, thrd_join, thrd_detach) WITHOUT including or depending
 *        on a real <threads.h> implementation -- backed by pthreads on
 *        Linux instead. Never #include <threads.h> anywhere in this
 *        project.
 *
 * Only the functions this codebase actually uses are implemented. Every
 * function below returns exactly what the underlying pthread_*() call
 * returned: 0 on success, pthread's own nonzero error code otherwise.
 */

/** @brief Spawns a new thread running start(arg). */
int thrd_create(thrd_t* thread, thrd_start_t start, void* arg);

/**
 * @brief Waits for @p thread to finish.
 * @param result  Out-param for the thread's return value, or NULL to
 *                discard it.
 */
int thrd_join(thrd_t thread, void** result);

/** @brief Detaches @p thread: its resources are released automatically on exit.
 */
int thrd_detach(thrd_t thread);

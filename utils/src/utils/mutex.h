#pragma once

#include "utils/mutex_linux.h"
// -- future Windows support: switch the include above between
//    "utils/mutex_linux.h" and a new "utils/mutex_windows.h" behind an
//    #ifdef _WIN32; both must define mtx_t/cnd_t. Not implemented yet.

/**
 * @file
 * @brief Mirrors the names of C11 <threads.h>'s mutex/condvar subset
 *        (mtx_t, mtx_init, ..., cnd_t, cnd_init, ...) WITHOUT including or
 *        depending on a real <threads.h> implementation -- backed by
 *        pthreads on Linux instead. Never #include <threads.h> anywhere in
 *        this project.
 *
 * Only the functions this codebase actually uses are implemented; e.g.
 * mtx_init() always creates a plain (non-recursive, non-timed) mutex, since
 * nothing here ever needs C11's mtx_type argument.
 *
 * Every function below returns exactly what the underlying pthread_*() call
 * returned: 0 on success, pthread's own nonzero error code otherwise.
 * Destroy functions are void, matching this codebase's *_destroy()
 * convention (log_destroy, list_destroy, ...).
 */

int mtx_init(mtx_t* mutex);
int mtx_lock(mtx_t* mutex);
int mtx_unlock(mtx_t* mutex);
void mtx_destroy(mtx_t* mutex);

int cnd_init(cnd_t* cond);
int cnd_wait(cnd_t* cond, mtx_t* mutex);
int cnd_signal(cnd_t* cond);
int cnd_broadcast(cnd_t* cond);
void cnd_destroy(cnd_t* cond);

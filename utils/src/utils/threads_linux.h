#pragma once

#include "utils/os.h"

#ifdef OS_LINUX

#include <pthread.h>

// Linux backing types for utils/threads.h. A future Windows backend would
// define the same two names (e.g. over HANDLE) in threads_windows.h,
// included from threads.h behind an #ifdef _WIN32 instead of this file.
//
// thrd_start_t deliberately keeps POSIX's void*-returning signature instead
// of C11's int-returning one: every thread function in this codebase already
// returns void*, and mirroring C11 exactly would force every thread function
// project-wide to be rewritten during the later migration pass.
typedef pthread_t thrd_t;
typedef void* (*thrd_start_t)(void*);

#endif  // OS_LINUX

#pragma once

#include <pthread.h>

// Linux backing types for utils/mutex.h. A future Windows backend would
// define the same two names (e.g. over CRITICAL_SECTION / CONDITION_VARIABLE)
// in mutex_windows.h, included from mutex.h behind an #ifdef _WIN32 instead
// of this file.
typedef pthread_mutex_t mtx_t;
typedef pthread_cond_t cnd_t;

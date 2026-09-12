#pragma once

#include <pthread.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

// Append `element` to `list` while holding `mutex`.
void list_add_mtx(t_list* list, pthread_mutex_t* mutex, void* element);

// Linear search for the process with this pid, guarded by `processes_mutex`.
// Returns NULL if there is none.
t_process* find_process(t_list* process_list, pthread_mutex_t* processes_mutex,
                        uint32_t pid);

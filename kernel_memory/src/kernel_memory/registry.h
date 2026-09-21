#pragma once

#include <stdint.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/mutex.h"

/** @brief Appends `element` to `list` while holding `mutex`. */
void list_add_mtx(t_list* list, mtx_t* mutex, void* element);

/**
 * @brief Linear search for the process with this pid, guarded by
 *        `processes_mutex`.
 * @return NULL if there is none.
 */
t_process* find_process(t_list* process_list, mtx_t* processes_mutex,
                        uint32_t pid);

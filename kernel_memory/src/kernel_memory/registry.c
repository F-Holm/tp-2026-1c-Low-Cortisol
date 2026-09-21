#include "kernel_memory/registry.h"

#include "utils/mutex.h"

void list_add_mtx(t_list* list, mtx_t* mutex, void* element)
{
  mtx_lock(mutex);
  list_add(list, element);
  mtx_unlock(mutex);
}

t_process* find_process(t_list* process_list, mtx_t* processes_mutex,
                        uint32_t pid)
{
  mtx_lock(processes_mutex);
  t_list_iterator* iterator = list_iterator_create(process_list);
  t_process* process_found = NULL;
  while (list_iterator_has_next(iterator))
  {
    t_process* process = list_iterator_next(iterator);
    if (process->pid == pid)
    {
      process_found = process;
      break;
    }
  }
  list_iterator_destroy(iterator);
  mtx_unlock(processes_mutex);
  return process_found;
}

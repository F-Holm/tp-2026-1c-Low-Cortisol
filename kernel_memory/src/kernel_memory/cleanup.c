#include "kernel_memory/cleanup.h"

#include <stdatomic.h>
#include <stdlib.h>

#include "kernel_memory/configurator.h"
#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"

void free_cpu_data(t_cpu_data* cpu_data)
{
  socket_destroy(cpu_data->socket_cpu);
  free(cpu_data);
}

void close_cpu(t_cpu_data* cpu)
{
  close_communication(cpu->socket_cpu);
}

void free_stick_data(t_stick_data* stick_data)
{
  if (stick_data == NULL)
    return;
  socket_destroy(stick_data->socket_stick);
  free(stick_data);
}

void free_swap_data(t_swap_data* swap_data)
{
  if (swap_data == NULL)
    return;
  if (swap_data->block_list != NULL)
  {
    list_destroy_and_destroy_elements(swap_data->block_list, free);
  }
  socket_destroy(swap_data->socket_swap);
  free(swap_data);
}

void free_scheduler_data(t_scheduler_data* scheduler_data)
{
  if (scheduler_data == NULL)
    return;
  // The scheduler socket stays allocated: other threads keep a pointer to
  // it, and free_kernel_memory_data() destroys it once they are all gone.
  close_communication(scheduler_data->socket_scheduler);
  socket_shutdown(scheduler_data->socket_kernel_memory, SOCKET_SHUTDOWN_BOTH);
  free(scheduler_data);
}

// Tells every connected peer (CPUs, the scheduler) we're going down, so
// their listener threads unblock from whatever they're reading and exit --
// that's what wait_for_listener_threads below is waiting on.
static void disconnect_all_peers(t_kernel_memory_data* kernel_data)
{
  if (kernel_data->connected_cpus != NULL)
  {
    for (int i = 0; i < list_size(kernel_data->connected_cpus); i++)
    {
      t_cpu_data* cpu = list_get(kernel_data->connected_cpus, i);
      socket_shutdown(cpu->socket_cpu, SOCKET_SHUTDOWN_BOTH);
    }
  }
  socket_shutdown(atomic_load(&(kernel_data->socket_scheduler)),
                  SOCKET_SHUTDOWN_BOTH);
}

static void wait_for_listener_threads(t_kernel_memory_data* kernel_data)
{
  mtx_lock(kernel_data->active_threads_mutex);
  while (kernel_data->active_threads > 0)
  {
    cnd_wait(kernel_data->active_threads_cond,
             kernel_data->active_threads_mutex);
  }
  mtx_unlock(kernel_data->active_threads_mutex);
}

static void free_kernel_memory_mutexes(t_kernel_memory_data* kernel_data)
{
  mtx_destroy(kernel_data->socket_list_mutex);
  mtx_destroy(kernel_data->processes_mutex);
  free(kernel_data->socket_list_mutex);
  free(kernel_data->processes_mutex);

  mtx_destroy(kernel_data->active_threads_mutex);
  cnd_destroy(kernel_data->active_threads_cond);
  free(kernel_data->active_threads_mutex);
  free(kernel_data->active_threads_cond);
}

static void free_connected_sticks(t_kernel_memory_data* kernel_data)
{
  if (kernel_data->connected_sticks == NULL)
    return;
  for (int i = 0; i < list_size(kernel_data->connected_sticks); i++)
  {
    t_stick_data* stick =
        (t_stick_data*)list_get(kernel_data->connected_sticks, i);
    free_stick_data(stick);
  }
  list_destroy(kernel_data->connected_sticks);
}

static void free_connected_cpus(t_kernel_memory_data* kernel_data)
{
  if (kernel_data->connected_cpus == NULL)
    return;
  while (!list_is_empty(kernel_data->connected_cpus))
  {
    free_cpu_data(list_remove(kernel_data->connected_cpus, 0));
  }
  list_destroy(kernel_data->connected_cpus);
}

static void free_all_processes(t_kernel_memory_data* kernel_data)
{
  if (kernel_data->processes == NULL)
    return;
  t_list_iterator* iterator = list_iterator_create(kernel_data->processes);
  while (list_iterator_has_next(iterator))
  {
    t_process* p = list_iterator_next(iterator);
    free_process(p);
  }
  list_iterator_destroy(iterator);
  list_destroy(kernel_data->processes);
}

void free_kernel_memory_data(t_kernel_memory_data* kernel_data)
{
  if (kernel_data == NULL)
    return;
  log_debug(kernel_data->logger,
            "Freeing kernel memory data and ending the program.");

  disconnect_all_peers(kernel_data);
  wait_for_listener_threads(kernel_data);
  free_kernel_memory_mutexes(kernel_data);

  free_connected_sticks(kernel_data);
  free_connected_cpus(kernel_data);
  free_swap_data(atomic_load(&kernel_data->swap_data));
  free_main_memory(kernel_data->main_memory);
  free_all_processes(kernel_data);

  socket_destroy(atomic_load(&(kernel_data->socket_scheduler)));
  socket_destroy(kernel_data->socket_kernel_memory);
  free(kernel_data);
}

void free_process(t_process* process)
{
  for (int i = 0; i < process->instruction_count; i++)
  {
    free(process->instructions[i]);
  }
  free(process->instructions);
  list_destroy_and_destroy_elements(process->segments, free);
  free(process);
}

void free_main_memory(t_main_memory* memory)
{
  if (memory == NULL)
    return;
  mtx_destroy(memory->main_memory_mutex);
  free(memory->main_memory_mutex);
  if (memory->segments != NULL)
    list_destroy_and_destroy_elements(memory->segments, free);
  if (memory->holes != NULL)
    list_destroy_and_destroy_elements(memory->holes, free);
  free(memory);
}

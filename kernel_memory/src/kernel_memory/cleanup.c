#include "kernel_memory/cleanup.h"

void free_cpu_data(t_cpu_data* cpu_data)
{
  close_cpu(cpu_data);
  free(cpu_data);
}

void close_cpu(t_cpu_data* cpu)
{
  if (cpu->socket_cpu != -1)
  {
    close_communication(cpu->socket_cpu);
  }
}

void free_stick_data(t_stick_data* stick_data)
{
  if (stick_data == NULL)
    return;
  close_communication(stick_data->socket_stick);
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
  if (swap_data->socket_swap > 0)
  {
    close_communication(swap_data->socket_swap);
  }
  free(swap_data);
}

void free_scheduler_data(t_scheduler_data* scheduler_data)
{
  if (scheduler_data == NULL)
    return;
  close_communication(scheduler_data->socket_scheduler);
  shutdown(scheduler_data->socket_kernel_memory, SHUT_RDWR);
  free(scheduler_data);
}

void free_kernel_memory_data(t_kernel_memory_data* kernel_data)
{
  log_info(kernel_data->logger,
           "Freeing kernel memory data and ending the program.");
  if (kernel_data == NULL)
    return;
  if (kernel_data->connected_cpus != NULL)
  {
    for (int i = 0; i < list_size(kernel_data->connected_cpus); i++)
    {
      t_cpu_data* cpu = list_get(kernel_data->connected_cpus, i);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
    }
  }
  if (kernel_data->socket_scheduler != -1)
  {
    shutdown(kernel_data->socket_scheduler, SHUT_RDWR);
  }
  pthread_mutex_lock(kernel_data->active_threads_mutex);
  while (kernel_data->active_threads > 0)
  {
    pthread_cond_wait(kernel_data->active_threads_cond,
                      kernel_data->active_threads_mutex);
  }
  pthread_mutex_unlock(kernel_data->active_threads_mutex);

  // Free mutexes
  pthread_mutex_destroy(kernel_data->socket_list_mutex);
  pthread_mutex_destroy(kernel_data->processes_mutex);
  free(kernel_data->socket_list_mutex);
  free(kernel_data->processes_mutex);

  pthread_mutex_destroy(kernel_data->active_threads_mutex);
  pthread_cond_destroy(kernel_data->active_threads_cond);
  free(kernel_data->active_threads_mutex);
  free(kernel_data->active_threads_cond);
  // Free the lists (only the structure, not the elements)
  if (kernel_data->connected_sticks != NULL)
  {
    for (int i = 0; i < list_size(kernel_data->connected_sticks); i++)
    {
      t_stick_data* stick =
          (t_stick_data*)list_get(kernel_data->connected_sticks, i);
      free_stick_data(stick);
    }
    list_destroy(kernel_data->connected_sticks);
  }
  if (kernel_data->connected_cpus != NULL)
  {
    /*for (int i = 0; i < list_size(kernel_data->connected_cpus); i++)
    {
      t_cpu_data* cpu =
          (t_cpu_data*)list_get(kernel_data->connected_cpus, i);
      //free_cpu_data(cpu);
      list_remove(kernel_data->connected_cpus, cpu);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
    }*/
    while (!list_is_empty(kernel_data->connected_cpus))
    {
      t_cpu_data* cpu = list_remove(kernel_data->connected_cpus, 0);
      shutdown(cpu->socket_cpu, SHUT_RDWR);
      free(cpu);
    }
    list_destroy(kernel_data->connected_cpus);
  }
  if (kernel_data->swap_data != NULL)
  {
    free_swap_data(kernel_data->swap_data);
  }
  if (kernel_data->main_memory != NULL)
  {
    free_main_memory(kernel_data->main_memory);
  }
  if (kernel_data->processes != NULL)
  {
    t_list_iterator* iterator = list_iterator_create(kernel_data->processes);
    while (list_iterator_has_next(iterator))
    {
      t_process* p = list_iterator_next(iterator);
      free_process(p);
    }
    list_iterator_destroy(iterator);
    list_destroy(kernel_data->processes);
  }
  if (kernel_data->socket_kernel_memory > 0)
  {
    close_communication(kernel_data->socket_kernel_memory);
  }

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
  pthread_mutex_destroy(memory->main_memory_mutex);
  free(memory->main_memory_mutex);
  if (memory->segments != NULL)
    list_destroy_and_destroy_elements(memory->segments, free);
  if (memory->holes != NULL)
    list_destroy_and_destroy_elements(memory->holes, free);
  free(memory);
}

#include "kernel_memory/holes.h"

#include <stdlib.h>

#include "kernel_memory/compaction.h"
#include "utils/msg.h"

static t_hole hole_selection_algorithm(
    uint32_t size, t_list* current_holes, t_log* logger,
    t_allocation_strategy allocation_strategy);
static void update_hole_table(t_main_memory* main_memory, t_hole chosen_hole,
                              uint32_t size);

int compute_free_space(t_list* holes, pthread_mutex_t* holes_mutex,
                       t_log* logger)
{
  int total = 0;
  log_trace(logger, "Calculating holes...");
  t_list_iterator* iterator = list_iterator_create(holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    total += current_hole->size;
  }
  list_iterator_destroy(iterator);
  log_trace(logger, "Free space: %d bytes", total);
  return total;
}

t_main_memory* add_total_memory(t_main_memory* main_memory, int memory_total)
{
  pthread_mutex_lock(main_memory->main_memory_mutex);
  int new_base = main_memory->total_size;
  main_memory->total_size += memory_total;

  t_hole* adjacent_hole = NULL;
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* h = list_iterator_next(iterator);
    if (h->base + h->size == new_base)
    {
      adjacent_hole = h;
      break;
    }
  }
  list_iterator_destroy(iterator);

  if (adjacent_hole != NULL)
  {
    adjacent_hole->size += memory_total;
  }
  else
  {
    t_hole* new_hole = malloc(sizeof(t_hole));
    new_hole->base = new_base;
    new_hole->size = memory_total;
    list_add(main_memory->holes, new_hole);
  }

  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return main_memory;
}

t_hole select_hole(uint32_t size, t_log* logger, t_main_memory* memory)
{
  t_hole chosen_hole = {-1, -1};
  if (memory->allocation_strategy == BEST)
    chosen_hole = hole_selection_algorithm(size, memory->holes, logger, BEST);
  else if (memory->allocation_strategy == WORST)
    chosen_hole = hole_selection_algorithm(size, memory->holes, logger, WORST);
  else
  {
    log_error(logger,
              "The chosen hole-selection option "
              "is not valid.");
    return chosen_hole;
  }
  update_hole_table(memory, chosen_hole, size);
  return chosen_hole;
}

void update_segment_list(t_main_memory* main_memory, t_hole chosen_hole,
                         int size, uint32_t pid, uint32_t id)
{
  t_segment* segment = malloc(sizeof(t_segment));
  segment->base = chosen_hole.base;
  segment->pid = pid;
  segment->id = id;
  segment->size = size;
  list_add(main_memory->segments, segment);
}

void create_segment(uint32_t id, uint32_t pid, int size,
                    t_main_memory* main_memory, int socket_scheduler,
                    t_log* logger)
{
  // Segmentation fault check
  if (size > main_memory->max_segment_size)
    send_string(OP_SEGMENT_SIZE_EXCEEDED,
                "The requested size exceeds "
                "the max segment size.",
                socket_scheduler);

  // Check available memory
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (compute_free_space(main_memory->holes, main_memory->main_memory_mutex,
                         logger) < size)
  {
    log_debug(logger, "Not enough space to create the segment");
    send_string(OP_NOT_ENOUGH_MEMORY, "There are not memory enough",
                socket_scheduler);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
  }
  else
  {
    log_debug(logger, "Creating segment with id %u, pid %u and size %d", id,
              pid, size);
    t_hole chosen_hole = select_hole(size, logger, main_memory);

    // Compaction check
    if (chosen_hole.size == -1)
    {
      log_debug(logger, "Memory needs to be compacted");
      notify_compaction(socket_scheduler);
      compact_memory(socket_scheduler, main_memory);
      chosen_hole = select_hole(size, logger, main_memory);
    }
    update_segment_list(main_memory, chosen_hole, size, pid, id);
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    send_string(OP_MEMORY_ALLOCATED, "Memory allocated", socket_scheduler);
    log_info(logger, "PID: %u - Segment created %u - Size: %d", pid, id, size);
  }
}

static t_hole hole_selection_algorithm(
    uint32_t size, t_list* current_holes, t_log* logger,
    t_allocation_strategy allocation_strategy)
{
  t_hole chosen_hole = {-1, -1};
  t_list_iterator* iterator = list_iterator_create(current_holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->size >= size)
    {
      switch (allocation_strategy)
      {
        case BEST:
          if (chosen_hole.size == -1 || chosen_hole.size > current_hole->size)
            chosen_hole = *current_hole;
          break;
        case WORST:
          if (chosen_hole.size == -1 || chosen_hole.size < current_hole->size)
            chosen_hole = *current_hole;
          break;
      }
    }
  }
  list_iterator_destroy(iterator);
  if (chosen_hole.size == -1)
  {
    log_debug(logger, "No available holes");
  }
  return chosen_hole;
}

static void update_hole_table(t_main_memory* main_memory, t_hole chosen_hole,
                              uint32_t size)
{
  t_list_iterator* iterator = list_iterator_create(main_memory->holes);
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->base == chosen_hole.base)
    {
      current_hole->size -= size;
      current_hole->base += size;
      if (current_hole->size == 0)
      {
        list_iterator_remove(iterator);
        free(current_hole);
      }
      break;
    }
  }
  list_iterator_destroy(iterator);
}

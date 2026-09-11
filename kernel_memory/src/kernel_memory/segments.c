#include "kernel_memory/segments.h"

#include <stdlib.h>

t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger)
{
  pthread_mutex_lock(main_memory->main_memory_mutex);

  log_trace(logger, "Searching %d segments for the one to remove",
            list_size(main_memory->segments));

  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  t_segment* found_segment = NULL;

  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    if (current_segment->id == id && current_segment->pid == pid)
    {
      list_iterator_remove(iterator);
      found_segment = current_segment;
      log_trace(logger, "removed the segment with ID: %d, PID: %d",
                found_segment->id, found_segment->pid);
      break;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  if (found_segment != NULL)
  {
    return found_segment;
  }
  log_error(logger,
            "an error occurred while removing the segment ("
            "found)");
  return NULL;
}

void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger)
{
  t_hole* new_hole = malloc(sizeof(t_hole));
  new_hole->base = 0;
  new_hole->size = 0;
  log_debug(logger, "removing requested segment ID: %d, PID: %d", id, pid);
  t_segment* segment_aux =
      find_and_remove_segment(id, pid, main_memory, logger);
  pthread_mutex_lock(main_memory->main_memory_mutex);
  if (segment_aux == NULL)
  {
    log_error(logger, "Segment not found");
    pthread_mutex_unlock(main_memory->main_memory_mutex);
    free(new_hole);
    return;
  }

  bool has_hole_before = hole_before_segment(
      segment_aux->base, segment_aux->base + segment_aux->size,
      main_memory->holes);
  bool has_hole_after = hole_after_segment(
      segment_aux->base, segment_aux->base + segment_aux->size,
      main_memory->holes);

  if (has_hole_before && has_hole_after)
  {
    int index1 = -1;
    int index2 = -1;
    // SEGMENT BETWEEN HOLES
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base + current_hole->size == segment_aux->base)
      {
        new_hole->base = current_hole->base;
        new_hole->size += current_hole->size + segment_aux->size;
        index1 = i;
      }
      if (current_hole->base == segment_aux->base + segment_aux->size)
      {
        new_hole->size += current_hole->size;
        index2 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    if (index1 == -1 || index2 == -1)
    {
      log_error(logger, "The holes adjacent to the segment were not found");
      pthread_mutex_unlock(main_memory->main_memory_mutex);
      free(segment_aux);
      free(new_hole);
      return;
    }
    if (index1 < index2)
    {
      list_remove_and_destroy_element(main_memory->holes, index2, free);
      list_remove_and_destroy_element(main_memory->holes, index1, free);
    }
    else
    {
      list_remove_and_destroy_element(main_memory->holes, index1, free);
      list_remove_and_destroy_element(main_memory->holes, index2, free);
    }
    list_add(main_memory->holes, new_hole);
    log_trace(logger, "Segment between holes");
  }
  else if (has_hole_before)
  {
    int index1 = -1;
    // SEGMENT AFTER HOLE
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base + current_hole->size == segment_aux->base)
      {
        new_hole->base = current_hole->base;
        new_hole->size = current_hole->size + segment_aux->size;
        index1 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, index1, free);
    list_add(main_memory->holes, new_hole);
    log_trace(logger, "Segment after hole");
  }
  else if (has_hole_after)
  {
    int index2 = -1;
    // SEGMENT BEFORE HOLE
    t_list_iterator* iterator = list_iterator_create(main_memory->holes);
    int i = 0;
    while (list_iterator_has_next(iterator))
    {
      t_hole* current_hole = list_iterator_next(iterator);
      if (current_hole->base == segment_aux->base + segment_aux->size)
      {
        new_hole->base = segment_aux->base;
        new_hole->size = current_hole->size + segment_aux->size;
        index2 = i;
      }
      i++;
    }
    list_iterator_destroy(iterator);

    list_remove_and_destroy_element(main_memory->holes, index2, free);
    list_add(main_memory->holes, new_hole);
    log_trace(logger, "Segment before hole");
  }
  else
  {
    new_hole->base = segment_aux->base;
    new_hole->size = segment_aux->size;
    list_add(main_memory->holes, new_hole);
    log_trace(logger, "Segment between two segments");
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  free(segment_aux);
}

bool hole_before_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool found = false;

  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);

    if ((current_hole->base + current_hole->size) == base_segment)
    {
      found = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return found;
}

bool hole_after_segment(int base_segment, int final_segment, t_list* holes)
{
  t_list_iterator* iterator = list_iterator_create(holes);
  bool found = false;
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->base == final_segment)
    {
      found = true;
      break;
    }
  }
  list_iterator_destroy(iterator);
  return found;
}

t_list* filter_process_segments(int pid, t_main_memory* main_memory,
                                t_log* logger)
{
  t_list* filtered_list = list_create();

  pthread_mutex_lock(main_memory->main_memory_mutex);
  for (int i = 0; i < list_size(main_memory->segments); i++)
  {
    t_segment* current_segment = list_get(main_memory->segments, i);
    if (current_segment->pid == pid)
    {
      list_add(filtered_list, current_segment);
    }
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);

  return filtered_list;
}

void add_segments_to_packet(t_list* segments, t_packet* process_segment_table)
{
  t_list_iterator* iterator = list_iterator_create(segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    packet_append(process_segment_table, current_segment, sizeof(t_segment));
  }
  list_iterator_destroy(iterator);
}

t_segment* find_segment(t_main_memory* main_memory, uint32_t pid,
                        uint32_t segment_number)
{
  t_segment* found_seg = NULL;
  uint32_t pid_segment_count = 0;

  pthread_mutex_lock(main_memory->main_memory_mutex);
  // scan the segments looking for the one matching this pid and segment number
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  while (list_iterator_has_next(iterator))
  {
    t_segment* current_item = list_iterator_next(iterator);
    if (current_item->pid == pid)
    {
      if (pid_segment_count == segment_number)
      {
        found_seg = current_item;
        break;
      }
      pid_segment_count++;
    }
  }
  list_iterator_destroy(iterator);
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  return found_seg;
}

int compute_process_size(t_process* process, t_main_memory* main_memory)
{
  int size = 0;
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  pthread_mutex_lock(main_memory->main_memory_mutex);
  while (list_iterator_has_next(iterator))
  {
    t_segment* segment_aux = list_iterator_next(iterator);
    if (segment_aux->pid == process->pid)
    {
      size += segment_aux->size;
    }
  }
  pthread_mutex_unlock(main_memory->main_memory_mutex);
  list_iterator_destroy(iterator);
  return size;
}

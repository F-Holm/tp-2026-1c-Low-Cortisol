#include "kernel_memory/segments.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/mutex.h"
#include "utils/registers_cpu.h"

t_segment* find_and_remove_segment(uint32_t id, uint32_t pid,
                                   t_main_memory* main_memory, t_log* logger)
{
  mtx_lock(main_memory->main_memory_mutex);

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
      log_trace(logger, "Removed the segment with ID: %d, PID: %d",
                found_segment->id, found_segment->pid);
      break;
    }
  }
  list_iterator_destroy(iterator);
  mtx_unlock(main_memory->main_memory_mutex);
  if (found_segment != NULL)
  {
    return found_segment;
  }
  log_error(logger, "Could not find the segment to remove (ID: %d, PID: %d)",
            id, pid);
  return NULL;
}

// Finds, in a single pass over `holes`, the index of the hole immediately
// before the segment [segment_base, segment_base+segment_size) and the index
// of the one immediately after it (or -1 for either that doesn't exist).
// remove_segment used to re-scan the list separately for each case (on top
// of hole_before_segment/hole_after_segment already having scanned it once
// each just to answer yes/no) -- one scan settles both at once.
static void find_adjacent_hole_indices(t_list* holes, int segment_base,
                                       int segment_size, int* index_before,
                                       int* index_after)
{
  int segment_end = segment_base + segment_size;
  *index_before = -1;
  *index_after = -1;
  t_list_iterator* iterator = list_iterator_create(holes);
  int i = 0;
  while (list_iterator_has_next(iterator))
  {
    t_hole* current_hole = list_iterator_next(iterator);
    if (current_hole->base + current_hole->size == segment_base)
      *index_before = i;
    if (current_hole->base == segment_end)
      *index_after = i;
    i++;
  }
  list_iterator_destroy(iterator);
}

void remove_segment(uint32_t id, uint32_t pid, t_main_memory* main_memory,
                    t_log* logger)
{
  log_debug(logger, "Removing requested segment ID: %d, PID: %d", id, pid);
  t_segment* segment_aux =
      find_and_remove_segment(id, pid, main_memory, logger);
  mtx_lock(main_memory->main_memory_mutex);
  if (segment_aux == NULL)
  {
    log_error(logger, "Segment not found");
    mtx_unlock(main_memory->main_memory_mutex);
    return;
  }

  int index_before, index_after;
  find_adjacent_hole_indices(main_memory->holes, segment_aux->base,
                             segment_aux->size, &index_before, &index_after);

  t_hole* new_hole = malloc(sizeof(t_hole));
  new_hole->base = segment_aux->base;
  new_hole->size = segment_aux->size;

  if (index_before != -1 && index_after != -1)
  {
    t_hole* hole_before = list_get(main_memory->holes, index_before);
    t_hole* hole_after = list_get(main_memory->holes, index_after);
    new_hole->base = hole_before->base;
    new_hole->size += hole_before->size + hole_after->size;
    // remove the higher index first, so the lower index stays valid
    if (index_before < index_after)
    {
      list_remove_and_destroy_element(main_memory->holes, index_after, free);
      list_remove_and_destroy_element(main_memory->holes, index_before, free);
    }
    else
    {
      list_remove_and_destroy_element(main_memory->holes, index_before, free);
      list_remove_and_destroy_element(main_memory->holes, index_after, free);
    }
    log_trace(logger, "Segment between holes");
  }
  else if (index_before != -1)
  {
    t_hole* hole_before = list_get(main_memory->holes, index_before);
    new_hole->base = hole_before->base;
    new_hole->size += hole_before->size;
    list_remove_and_destroy_element(main_memory->holes, index_before, free);
    log_trace(logger, "Segment after hole");
  }
  else if (index_after != -1)
  {
    t_hole* hole_after = list_get(main_memory->holes, index_after);
    new_hole->size += hole_after->size;
    list_remove_and_destroy_element(main_memory->holes, index_after, free);
    log_trace(logger, "Segment before hole");
  }
  else
  {
    log_trace(logger, "Segment between two segments");
  }
  list_add(main_memory->holes, new_hole);

  mtx_unlock(main_memory->main_memory_mutex);
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

  mtx_lock(main_memory->main_memory_mutex);
  for (int i = 0; i < list_size(main_memory->segments); i++)
  {
    t_segment* current_segment = list_get(main_memory->segments, i);
    if (current_segment->pid == pid)
    {
      list_add(filtered_list, current_segment);
    }
  }
  mtx_unlock(main_memory->main_memory_mutex);

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

  mtx_lock(main_memory->main_memory_mutex);
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
  mtx_unlock(main_memory->main_memory_mutex);
  return found_seg;
}

int compute_process_size(t_process* process, t_main_memory* main_memory)
{
  int size = 0;
  t_list_iterator* iterator = list_iterator_create(main_memory->segments);
  mtx_lock(main_memory->main_memory_mutex);
  while (list_iterator_has_next(iterator))
  {
    t_segment* segment_aux = list_iterator_next(iterator);
    if (segment_aux->pid == process->pid)
    {
      size += segment_aux->size;
    }
  }
  mtx_unlock(main_memory->main_memory_mutex);
  list_iterator_destroy(iterator);
  return size;
}

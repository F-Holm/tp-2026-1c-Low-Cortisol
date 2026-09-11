#include "kernel_memory/compaction.h"

#include <stdlib.h>
#include <unistd.h>

#include "utils/msg.h"

bool compact_memory(int socket_scheduler, t_main_memory* main_memory)
{
  compact_segments(main_memory->segments);
  list_destroy_and_destroy_elements(main_memory->holes, free);
  main_memory->holes = compact_holes(
      main_memory->total_size, compute_last_segment_end(main_memory->segments));
  usleep(main_memory->compaction_delay * 1000);
  send_string(OP_COMPACTION_DONE, "Compaction finished", socket_scheduler);
  return true;
}

void compact_segments(t_list* segments)
{
  t_list_iterator* iterator = list_iterator_create(segments);

  if (list_iterator_has_next(iterator))
  {
    t_segment* current_segment = list_iterator_next(iterator);
    current_segment->base = 0;

    while (list_iterator_has_next(iterator))
    {
      t_segment* next_segment = list_iterator_next(iterator);

      next_segment->base = current_segment->base + current_segment->size;

      current_segment = next_segment;
    }
  }
  list_iterator_destroy(iterator);
}

int compute_last_segment_end(t_list* segments)
{
  t_segment* last_segment = list_get(segments, list_size(segments) - 1);
  return last_segment->base + last_segment->size;
}

t_list* compact_holes(int memory_total, int base_final_segment)
{
  t_list* holes = list_create();
  t_hole* hole_final = malloc(sizeof(t_hole));
  hole_final->base = base_final_segment;
  hole_final->size = memory_total - base_final_segment;
  list_add(holes, hole_final);
  return holes;
}

void notify_compaction(int socket_scheduler)
{
  send_string(OP_COMPACTION_NEEDED, "Memory needs to be compacted",
              socket_scheduler);
  int op_code = receive_op_code(socket_scheduler);
  if (op_code == OP_CAN_COMPACT)
  {
    char* message = receive_string(socket_scheduler);
    free(message);
  }
}

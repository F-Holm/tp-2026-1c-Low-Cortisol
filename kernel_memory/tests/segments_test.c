#include "kernel_memory/segments.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"

Test(km_segments, hole_adjacency_checks)
{
  t_list* holes = list_create();
  list_add(holes, km_make_hole(0, 50));   /* ends at 50 */
  list_add(holes, km_make_hole(150, 30)); /* starts at 150 */

  /* segment [50, 150) has a hole right before it and right after it */
  cr_assert(hole_before_segment(50, 150, holes));
  cr_assert(hole_after_segment(50, 150, holes));

  /* segment [60, 140) touches neither */
  cr_assert_not(hole_before_segment(60, 140, holes));
  cr_assert_not(hole_after_segment(60, 140, holes));

  list_destroy_and_destroy_elements(holes, free);
}

Test(km_segments, compute_process_size_adds_only_the_owned_segments)
{
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 100));
  list_add(memory->segments, km_make_segment(1, 1, 100, 50));
  list_add(memory->segments, km_make_segment(0, 2, 150, 999));

  t_process process = {.pid = 1};
  cr_assert_eq(compute_process_size(&process, memory), 150);

  free_main_memory(memory);
}

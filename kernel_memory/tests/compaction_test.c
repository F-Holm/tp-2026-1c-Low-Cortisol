#include "kernel_memory/compaction.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "support.h"
#include "utils/collections/list.h"

Test(km_compaction, compute_last_segment_end_is_base_plus_size_of_the_last)
{
  t_list* segments = list_create();
  list_add(segments, km_make_segment(0, 1, 0, 64));
  list_add(segments, km_make_segment(1, 1, 64, 32));
  cr_assert_eq(compute_last_segment_end(segments), 96);
  list_destroy_and_destroy_elements(segments, free);
}

Test(km_compaction, compact_holes_leaves_one_hole_after_the_last_segment)
{
  t_list* holes = compact_holes(1000, 640);
  cr_assert_eq(list_size(holes), 1);
  t_hole* hole = list_get(holes, 0);
  cr_assert_eq(hole->base, 640);
  cr_assert_eq(hole->size, 360);
  list_destroy_and_destroy_elements(holes, free);
}

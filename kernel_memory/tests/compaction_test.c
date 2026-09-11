#include "kernel_memory/compaction.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/* ── compact_segments ──────────────────────────────────────────────────── */

Test(km_compaction, compact_segments_slides_everything_down_contiguously)
{
  t_list* segments = list_create();
  t_segment* a = km_make_segment(0, 1, 100, 50);
  t_segment* b = km_make_segment(1, 1, 300, 20);
  t_segment* c = km_make_segment(2, 1, 500, 10);
  list_add(segments, a);
  list_add(segments, b);
  list_add(segments, c);

  compact_segments(segments);

  cr_assert_eq(a->base, 0);
  cr_assert_eq(b->base, 50);
  cr_assert_eq(c->base, 70);

  list_destroy_and_destroy_elements(segments, free);
}

Test(km_compaction, compact_segments_is_a_no_op_for_an_empty_list)
{
  t_list* segments = list_create();
  compact_segments(segments); /* must not crash */
  list_destroy(segments);
}

/* ── notify_compaction ─────────────────────────────────────────────────── */

Test(km_compaction, notify_compaction_asks_and_drains_the_reply)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_CAN_COMPACT, "go ahead", server_fd));

  notify_compaction(client_fd);

  cr_assert_eq(receive_op_code(server_fd), OP_COMPACTION_NEEDED);
  free(receive_string(server_fd));

  close(client_fd);
  close(server_fd);
}

/* ── compact_memory ────────────────────────────────────────────────────── */

Test(km_compaction, compact_memory_compacts_and_notifies_the_scheduler)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);

  t_main_memory* memory = init_main_memory(1000, BEST, 0);
  memory->total_size = 1000;
  list_add(memory->segments, km_make_segment(0, 1, 100, 50)); /* gap before */
  list_add(memory->segments, km_make_segment(1, 1, 300, 20)); /* gap between */
  list_add(memory->holes, km_make_hole(0, 100));
  list_add(memory->holes, km_make_hole(150, 150));
  list_add(memory->holes, km_make_hole(320, 680));

  cr_assert(compact_memory(client_fd, memory));

  t_segment* first = list_get(memory->segments, 0);
  t_segment* second = list_get(memory->segments, 1);
  cr_assert_eq(first->base, 0);
  cr_assert_eq(second->base, 50);
  cr_assert_eq(list_size(memory->holes), 1);
  t_hole* hole = list_get(memory->holes, 0);
  cr_assert_eq(hole->base, 70);
  cr_assert_eq(hole->size, 930);

  cr_assert_eq(receive_op_code(server_fd), OP_COMPACTION_DONE);
  free(receive_string(server_fd));

  free_main_memory(memory);
  close(client_fd);
  close(server_fd);
}

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

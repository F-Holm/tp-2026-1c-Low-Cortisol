#include "kernel_memory/segments.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

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

/* ── find_and_remove_segment ───────────────────────────────────────────── */

Test(km_segments, find_and_remove_segment_removes_the_matching_segment)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 100));
  list_add(memory->segments, km_make_segment(1, 1, 100, 50));

  t_segment* found = find_and_remove_segment(1, 1, memory, logger);

  cr_assert_not_null(found);
  cr_assert_eq(found->id, 1);
  cr_assert_eq(list_size(memory->segments), 1);

  free(found);
  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_segments, find_and_remove_segment_reports_not_found)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 100));

  cr_assert_null(find_and_remove_segment(99, 1, memory, logger));
  cr_assert_eq(list_size(memory->segments), 1);

  free_main_memory(memory);
  log_destroy(logger);
}

/* ── remove_segment ────────────────────────────────────────────────────── */

Test(km_segments, remove_segment_merges_two_adjacent_holes)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 50, 100)); /* [50, 150) */
  list_add(memory->holes, km_make_hole(0, 50));               /* ends at 50 */
  list_add(memory->holes, km_make_hole(150, 30)); /* starts at 150 */

  remove_segment(0, 1, memory, logger);

  cr_assert_eq(list_size(memory->segments), 0);
  cr_assert_eq(list_size(memory->holes), 1);
  t_hole* merged = list_get(memory->holes, 0);
  cr_assert_eq(merged->base, 0);
  cr_assert_eq(merged->size, 180);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_segments, remove_segment_merges_with_a_hole_before_it)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 50, 100)); /* [50, 150) */
  list_add(memory->holes, km_make_hole(0, 50));               /* ends at 50 */

  remove_segment(0, 1, memory, logger);

  cr_assert_eq(list_size(memory->holes), 1);
  t_hole* merged = list_get(memory->holes, 0);
  cr_assert_eq(merged->base, 0);
  cr_assert_eq(merged->size, 150);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_segments, remove_segment_merges_with_a_hole_after_it)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 50, 100)); /* [50, 150) */
  list_add(memory->holes, km_make_hole(150, 30)); /* starts at 150 */

  remove_segment(0, 1, memory, logger);

  cr_assert_eq(list_size(memory->holes), 1);
  t_hole* merged = list_get(memory->holes, 0);
  cr_assert_eq(merged->base, 50);
  cr_assert_eq(merged->size, 130);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_segments, remove_segment_becomes_a_standalone_hole_with_no_neighbors)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 50, 100)); /* [50, 150) */

  remove_segment(0, 1, memory, logger);

  cr_assert_eq(list_size(memory->holes), 1);
  t_hole* new_hole = list_get(memory->holes, 0);
  cr_assert_eq(new_hole->base, 50);
  cr_assert_eq(new_hole->size, 100);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_segments, remove_segment_is_a_no_op_when_the_segment_is_missing)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);

  remove_segment(0, 1, memory, logger); /* must not crash */

  cr_assert_eq(list_size(memory->holes), 0);

  free_main_memory(memory);
  log_destroy(logger);
}

/* ── find_segment ──────────────────────────────────────────────────────── */

Test(km_segments, find_segment_returns_the_nth_segment_for_a_pid)
{
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  t_segment* pid1_first = km_make_segment(0, 1, 0, 10);
  t_segment* pid2_first = km_make_segment(0, 2, 10, 10);
  t_segment* pid1_second = km_make_segment(1, 1, 20, 10);
  list_add(memory->segments, pid1_first);
  list_add(memory->segments, pid2_first);
  list_add(memory->segments, pid1_second);

  cr_assert_eq(find_segment(memory, 1, 0), pid1_first);
  cr_assert_eq(find_segment(memory, 1, 1), pid1_second);
  cr_assert_null(find_segment(memory, 1, 2));
  cr_assert_null(find_segment(memory, 3, 0));

  free_main_memory(memory);
}

/* ── filter_process_segments ───────────────────────────────────────────── */

Test(km_segments, filter_process_segments_keeps_only_the_matching_pid)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  t_segment* mine = km_make_segment(0, 1, 0, 10);
  list_add(memory->segments, mine);
  list_add(memory->segments, km_make_segment(0, 2, 10, 10));

  t_list* filtered = filter_process_segments(1, memory, logger);

  cr_assert_eq(list_size(filtered), 1);
  cr_assert_eq(list_get(filtered, 0), mine);

  list_destroy(filtered);
  free_main_memory(memory);
  log_destroy(logger);
}

/* ── add_segments_to_packet ────────────────────────────────────────────── */

Test(km_segments, add_segments_to_packet_appends_every_segment)
{
  t_list* segments = list_create();
  list_add(segments, km_make_segment(0, 1, 0, 10));
  list_add(segments, km_make_segment(1, 1, 10, 20));

  t_packet* packet = create_packet(OP_SEGMENT_TABLE);
  add_segments_to_packet(segments, packet);

  cr_assert_eq(packet->buffer->size, 2 * (sizeof(t_segment) + sizeof(int)));

  destroy_packet(packet);
  list_destroy_and_destroy_elements(segments, free);
}

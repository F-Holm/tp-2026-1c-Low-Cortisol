#include "kernel_memory/protocol.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── list_add_mtx ──────────────────────────────────────────────────────── */

Test(km_protocol, list_add_mtx_appends_under_the_lock)
{
  t_list* list = list_create();
  int a = 1;
  int b = 2;
  list_add_mtx(list, &mutex, &a);
  list_add_mtx(list, &mutex, &b);
  cr_assert_eq(list_size(list), 2);
  cr_assert_eq(list_get(list, 1), &b);
  list_destroy(list);
}

/* ── compute_total_memory / compute_free_space ────────────────────────── */

Test(km_protocol, compute_total_memory_sums_the_stick_sizes)
{
  t_list* sticks = list_create();
  list_add(sticks, km_make_stick(256));
  list_add(sticks, km_make_stick(512));
  cr_assert_eq(compute_total_memory(sticks, &mutex), 768);
  list_destroy_and_destroy_elements(sticks, free);
}

Test(km_protocol, compute_free_space_sums_the_hole_sizes)
{
  t_log* logger = km_quiet_logger();
  t_list* holes = list_create();
  list_add(holes, km_make_hole(0, 100));
  list_add(holes, km_make_hole(200, 40));
  cr_assert_eq(compute_free_space(holes, &mutex, logger), 140);
  list_destroy_and_destroy_elements(holes, free);
  log_destroy(logger);
}

/* ── compute_last_segment_end ─────────────────────────────────────────── */

Test(km_protocol, compute_last_segment_end_is_base_plus_size_of_the_last)
{
  t_list* segments = list_create();
  list_add(segments, km_make_segment(0, 1, 0, 64));
  list_add(segments, km_make_segment(1, 1, 64, 32));
  cr_assert_eq(compute_last_segment_end(segments), 96);
  list_destroy_and_destroy_elements(segments, free);
}

/* ── find_process ─────────────────────────────────────────────────────── */

Test(km_protocol, find_process_matches_by_pid)
{
  t_list* processes = list_create();
  t_process p1 = {.pid = 1};
  t_process p2 = {.pid = 2};
  list_add(processes, &p1);
  list_add(processes, &p2);

  cr_assert_eq(find_process(processes, &mutex, 2), &p2);
  cr_assert_null(find_process(processes, &mutex, 9));

  list_destroy(processes);
}

/* ── hole_before_segment / hole_after_segment ─────────────────────────── */

Test(km_protocol, hole_adjacency_checks)
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

/* ── compact_holes ────────────────────────────────────────────────────── */

Test(km_protocol, compact_holes_leaves_one_hole_after_the_last_segment)
{
  t_list* holes = compact_holes(1000, 640);
  cr_assert_eq(list_size(holes), 1);
  t_hole* hole = list_get(holes, 0);
  cr_assert_eq(hole->base, 640);
  cr_assert_eq(hole->size, 360);
  list_destroy_and_destroy_elements(holes, free);
}

/* ── add_total_memory ─────────────────────────────────────────────────── */

Test(km_protocol, add_total_memory_grows_the_size_and_adds_a_tail_hole)
{
  t_main_memory* memory = init_main_memory(4096, BEST, 0);

  add_total_memory(memory, 1024);
  cr_assert_eq(memory->total_size, 1024);
  cr_assert_eq(list_size(memory->holes), 1);
  cr_assert_eq(((t_hole*)list_get(memory->holes, 0))->base, 0);
  cr_assert_eq(((t_hole*)list_get(memory->holes, 0))->size, 1024);

  /* a second stick extends the trailing hole instead of adding a new one */
  add_total_memory(memory, 512);
  cr_assert_eq(memory->total_size, 1536);
  cr_assert_eq(list_size(memory->holes), 1);
  cr_assert_eq(((t_hole*)list_get(memory->holes, 0))->size, 1536);

  free_main_memory(memory);
}

/* ── select_hole ──────────────────────────────────────────────────────── */

Test(km_select_hole, best_fit_picks_the_smallest_hole_that_fits)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 100));
  list_add(memory->holes, km_make_hole(100, 40));
  list_add(memory->holes, km_make_hole(140, 200));

  t_hole chosen = select_hole(30, logger, memory);
  cr_assert_eq(chosen.base, 100); /* the 40-byte hole */

  /* the chosen hole shrank and moved forward */
  t_hole* remainder = list_get(memory->holes, 1);
  cr_assert_eq(remainder->base, 130);
  cr_assert_eq(remainder->size, 10);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_select_hole, worst_fit_picks_the_largest_hole)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, WORST, 0);
  list_add(memory->holes, km_make_hole(0, 100));
  list_add(memory->holes, km_make_hole(100, 40));
  list_add(memory->holes, km_make_hole(140, 200));

  t_hole chosen = select_hole(30, logger, memory);
  cr_assert_eq(chosen.base, 140); /* the 200-byte hole */

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_select_hole, an_exact_fit_removes_the_hole)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 64));

  t_hole chosen = select_hole(64, logger, memory);
  cr_assert_eq(chosen.base, 0);
  cr_assert_eq(list_size(memory->holes), 0);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_select_hole, reports_no_hole_when_nothing_fits)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 8));

  t_hole chosen = select_hole(64, logger, memory);
  cr_assert_eq(chosen.size, -1);

  free_main_memory(memory);
  log_destroy(logger);
}

/* ── compute_process_size ─────────────────────────────────────────────── */

Test(km_protocol, compute_process_size_adds_only_the_owned_segments)
{
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 100));
  list_add(memory->segments, km_make_segment(1, 1, 100, 50));
  list_add(memory->segments, km_make_segment(0, 2, 150, 999));

  t_process process = {.pid = 1};
  cr_assert_eq(compute_process_size(&process, memory), 150);

  free_main_memory(memory);
}

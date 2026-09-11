#include "kernel_memory/holes.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

Test(km_holes, compute_free_space_sums_the_hole_sizes)
{
  t_log* logger = km_quiet_logger();
  t_list* holes = list_create();
  list_add(holes, km_make_hole(0, 100));
  list_add(holes, km_make_hole(200, 40));
  cr_assert_eq(compute_free_space(holes, &mutex, logger), 140);
  list_destroy_and_destroy_elements(holes, free);
  log_destroy(logger);
}

Test(km_holes, add_total_memory_grows_the_size_and_adds_a_tail_hole)
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

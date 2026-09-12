#include "kernel_memory/holes.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

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

Test(km_select_hole, reports_an_error_for_an_unrecognized_strategy)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  memory->allocation_strategy = 99; /* neither BEST nor WORST */
  list_add(memory->holes, km_make_hole(0, 100));

  t_hole chosen = select_hole(30, logger, memory);
  cr_assert_eq(chosen.base, -1);
  cr_assert_eq(chosen.size, -1);

  free_main_memory(memory);
  log_destroy(logger);
}

/* ── update_segment_list ───────────────────────────────────────────────── */

Test(km_holes, update_segment_list_appends_a_new_segment)
{
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  t_hole chosen = {.base = 128, .size = 999};

  update_segment_list(memory, chosen, 64, 7, 3);

  cr_assert_eq(list_size(memory->segments), 1);
  t_segment* segment = list_get(memory->segments, 0);
  cr_assert_eq(segment->base, 128);
  cr_assert_eq(segment->pid, 7);
  cr_assert_eq(segment->id, 3);
  cr_assert_eq(segment->size, 64);

  free_main_memory(memory);
}

/* ── create_segment ────────────────────────────────────────────────────── */

Test(km_create_segment, succeeds_when_a_hole_fits)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 100));

  create_segment(1, 1, 64, memory, client_fd, logger);

  cr_assert_eq(receive_op_code(server_fd), OP_MEMORY_ALLOCATED);
  free(receive_string(server_fd));
  cr_assert_eq(list_size(memory->segments), 1);

  free_main_memory(memory);
  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_create_segment, reports_not_enough_memory)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 8));

  create_segment(1, 1, 64, memory, client_fd, logger);

  cr_assert_eq(receive_op_code(server_fd), OP_NOT_ENOUGH_MEMORY);
  free(receive_string(server_fd));
  cr_assert_eq(list_size(memory->segments), 0);

  free_main_memory(memory);
  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_create_segment, rejects_a_segment_larger_than_the_max_size)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(32, BEST, 0);
  list_add(memory->holes, km_make_hole(0, 1000));

  create_segment(1, 1, 64, memory, client_fd, logger);

  /* create_segment doesn't return after flagging the oversized segment --
   * it falls through and still allocates it if there's room, so both
   * messages arrive. */
  cr_assert_eq(receive_op_code(server_fd), OP_SEGMENT_SIZE_EXCEEDED);
  free(receive_string(server_fd));
  cr_assert_eq(receive_op_code(server_fd), OP_MEMORY_ALLOCATED);
  free(receive_string(server_fd));

  free_main_memory(memory);
  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_create_segment, compacts_memory_when_no_single_hole_is_big_enough)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_CAN_COMPACT, "go ahead", server_fd));

  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(4096, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 10));
  list_add(memory->holes, km_make_hole(10, 20)); /* too small alone */
  list_add(memory->holes, km_make_hole(30, 20)); /* too small alone */
  memory->total_size = 50;

  create_segment(1, 2, 35, memory, client_fd, logger);

  cr_assert_eq(receive_op_code(server_fd), OP_COMPACTION_NEEDED);
  free(receive_string(server_fd));
  cr_assert_eq(receive_op_code(server_fd), OP_COMPACTION_DONE);
  free(receive_string(server_fd));
  cr_assert_eq(receive_op_code(server_fd), OP_MEMORY_ALLOCATED);
  free(receive_string(server_fd));
  cr_assert_eq(list_size(memory->segments), 2);

  free_main_memory(memory);
  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

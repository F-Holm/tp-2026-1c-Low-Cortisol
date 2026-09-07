#include "cpu/memory.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/list.h"

/* ── find_segment_by_id ────────────────────────────────────────────────── */

Test(cpu_memory, find_segment_by_id_returns_the_matching_segment)
{
  t_list* table = list_create();
  list_add(table, cpu_make_segment(0, 100, 64));
  list_add(table, cpu_make_segment(1, 200, 64));
  list_add(table, cpu_make_segment(2, 300, 64));

  t_segment* found = find_segment_by_id(table, 1);
  cr_assert_not_null(found);
  cr_assert_eq(found->base, 200);

  cr_assert_null(find_segment_by_id(table, 9));

  list_destroy_and_destroy_elements(table, free);
}

/* ── find_stick ────────────────────────────────────────────────────────── */

Test(cpu_memory, find_stick_matches_the_owning_address_range)
{
  t_cpu cpu = {0};
  cpu.memory_sticks = list_create();
  list_add(cpu.memory_sticks, cpu_make_stick(0, 100));
  list_add(cpu.memory_sticks, cpu_make_stick(100, 100));

  cr_assert_eq(find_stick(&cpu, 0)->offset, 0);
  cr_assert_eq(find_stick(&cpu, 99)->offset, 0);
  cr_assert_eq(find_stick(&cpu, 100)->offset, 100);
  cr_assert_eq(find_stick(&cpu, 199)->offset, 100);
  cr_assert_null(find_stick(&cpu, 200));

  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
}

/* ── mmu ───────────────────────────────────────────────────────────────── */

Test(cpu_mmu, translates_a_logical_address_to_base_plus_offset)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(1, 500, 64));

  /* logical 150 -> segment 1, offset 50 -> base 500 + 50 */
  cr_assert_eq(mmu(&cpu, context, 150, 4, 1), 550);

  cpu_destroy_context(context);
  log_destroy(cpu.logger);
}

Test(cpu_mmu, reports_a_missing_segment_as_an_error)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;

  t_context* context = cpu_make_context(); /* empty table */

  cr_assert_eq(mmu(&cpu, context, 150, 4, 1), INVALID_ADDRESS - 1);

  cpu_destroy_context(context);
  log_destroy(cpu.logger);
}

Test(cpu_mmu, a_read_past_the_segment_end_raises_a_segmentation_fault)
{
  int scheduler_fd;
  int cpu_fd = cpu_connected_pair(&scheduler_fd);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;
  cpu.socket_kernel_scheduler = cpu_fd;

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));

  /* offset 30 + size 4 = 34 > segment size 32 */
  cr_assert_eq(mmu(&cpu, context, 30, 4, 7), INVALID_ADDRESS);

  cr_assert_eq(receive_op_code(scheduler_fd), OP_SEG_FAULT);
  char* reason = receive_string(scheduler_fd);
  cr_assert_str_eq(reason, "SEGMENTATION FAULT");
  free(reason);

  cpu_destroy_context(context);
  close(cpu_fd);
  close(scheduler_fd);
  log_destroy(cpu.logger);
}

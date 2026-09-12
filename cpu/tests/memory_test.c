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

/* ── request_read / receive_read_response ─────────────────────────────── */

Test(cpu_memory, request_read_sends_the_offset_and_byte_count)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(request_read(&cpu, stick, 12, 8));

  cr_assert_eq(receive_op_code(peer_fd), OP_MEMORY_STICK_READ);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(list_size(fields), 2);
  cr_assert_eq(*(int*)list_get(fields, 0), 12);
  cr_assert_eq(*(int*)list_get(fields, 1), 8);
  list_destroy_and_destroy_elements(fields, free);

  free(stick);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_read_response_returns_the_data_on_success)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "ABCDEFGH", peer_fd));

  char* data = receive_read_response(&cpu, stick);
  cr_assert_not_null(data);
  cr_assert_str_eq(data, "ABCDEFGH");
  free(data);

  free(stick);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_read_response_fails_on_an_unexpected_op_code)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  int km_fd;
  int km_peer_fd = cpu_connected_pair(&km_fd);
  t_cpu cpu = {.socket_kernel_memory = km_fd};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_CODE_ERROR, "nope", peer_fd));

  cr_assert_null(receive_read_response(&cpu, stick));

  free(stick);
  close(stick_fd);
  close(peer_fd);
  close(km_fd);
  close(km_peer_fd);
  log_destroy(cpu.logger);
}

/* ── read_memory ───────────────────────────────────────────────────────── */

Test(cpu_memory, read_memory_reads_from_a_single_stick)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;
  list_add(cpu.memory_sticks, stick);

  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "HELLO!!!", peer_fd));

  void* result = read_memory(&cpu, 4, 8);
  cr_assert_not_null(result);
  cr_assert_eq(memcmp(result, "HELLO!!!", 8), 0);
  free(result);

  cr_assert_eq(receive_op_code(peer_fd), OP_MEMORY_STICK_READ);
  free(receive_packet(peer_fd)); /* drain the request, ignore its shape here */

  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, read_memory_spans_two_sticks)
{
  int peer_fd_a;
  int stick_fd_a = cpu_connected_pair(&peer_fd_a);
  int peer_fd_b;
  int stick_fd_b = cpu_connected_pair(&peer_fd_b);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();
  t_memory_stick_info* stick_a = cpu_make_stick(0, 4);
  stick_a->socket_ms = stick_fd_a;
  t_memory_stick_info* stick_b = cpu_make_stick(4, 4);
  stick_b->socket_ms = stick_fd_b;
  list_add(cpu.memory_sticks, stick_a);
  list_add(cpu.memory_sticks, stick_b);

  /* address 2, size 4: 2 bytes from stick A (offset 2..3), 2 from stick B
   * (offset 0..1). */
  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "AB", peer_fd_a));
  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "CD", peer_fd_b));

  void* result = read_memory(&cpu, 2, 4);
  cr_assert_not_null(result);
  cr_assert_eq(memcmp(result, "ABCD", 4), 0);
  free(result);

  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  close(stick_fd_a);
  close(peer_fd_a);
  close(stick_fd_b);
  close(peer_fd_b);
  log_destroy(cpu.logger);
}

/* ── request_write / receive_write_response ───────────────────────────── */

Test(cpu_memory, request_write_sends_the_offset_and_the_bytes)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  char payload[4] = "hey";
  cr_assert(request_write(&cpu, stick, 6, payload, sizeof(payload)));

  cr_assert_eq(receive_op_code(peer_fd), OP_MEMORY_STICK_WRITE);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(list_size(fields), 3);
  cr_assert_eq(*(int*)list_get(fields, 0), 6);
  cr_assert_eq(memcmp(list_get(fields, 1), payload, sizeof(payload)), 0);
  cr_assert_eq(*(int*)list_get(fields, 2), (int)sizeof(payload));
  list_destroy_and_destroy_elements(fields, free);

  free(stick);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_write_response_succeeds_on_write_done)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", peer_fd));
  cr_assert(receive_write_response(&cpu, stick));

  free(stick);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_write_response_fails_when_the_stick_disconnected)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  int km_fd;
  int km_peer_fd = cpu_connected_pair(&km_fd);
  t_cpu cpu = {.socket_kernel_memory = km_fd};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_CODE_ERROR, "disconnected", peer_fd));
  cr_assert_not(receive_write_response(&cpu, stick));

  free(stick);
  close(stick_fd);
  close(peer_fd);
  close(km_fd);
  close(km_peer_fd);
  log_destroy(cpu.logger);
}

/* ── write_memory ──────────────────────────────────────────────────────── */

Test(cpu_memory, write_memory_writes_to_a_single_stick)
{
  int peer_fd;
  int stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;
  list_add(cpu.memory_sticks, stick);

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", peer_fd));

  char payload[4] = "yo!";
  cr_assert(write_memory(&cpu, 10, payload, sizeof(payload)));

  cr_assert_eq(receive_op_code(peer_fd), OP_MEMORY_STICK_WRITE);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(*(int*)list_get(fields, 0), 10);
  cr_assert_eq(memcmp(list_get(fields, 1), payload, sizeof(payload)), 0);
  list_destroy_and_destroy_elements(fields, free);

  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  close(stick_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

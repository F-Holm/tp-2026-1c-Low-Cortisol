#include "cpu/memory.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/sockets.h"

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
  t_socket* scheduler_fd;
  t_socket* cpu_fd = cpu_connected_pair(&scheduler_fd);

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
  socket_destroy(cpu_fd);
  socket_destroy(scheduler_fd);
  log_destroy(cpu.logger);
}

/* ── request_read / receive_read_response ─────────────────────────────── */

Test(cpu_memory, request_read_sends_the_offset_and_byte_count)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_read_response_returns_the_data_on_success)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_read_response_fails_on_an_unexpected_op_code)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
  t_socket* km_fd;
  t_socket* km_peer_fd = cpu_connected_pair(&km_fd);
  t_cpu cpu = {.socket_kernel_memory = km_fd};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_CODE_ERROR, "nope", peer_fd));

  cr_assert_null(receive_read_response(&cpu, stick));

  free(stick);
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  socket_destroy(km_fd);
  socket_destroy(km_peer_fd);
  log_destroy(cpu.logger);
}

/* ── read_memory ───────────────────────────────────────────────────────── */

Test(cpu_memory, read_memory_reads_from_a_single_stick)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, read_memory_spans_two_sticks)
{
  t_socket* peer_fd_a;
  t_socket* stick_fd_a = cpu_connected_pair(&peer_fd_a);
  t_socket* peer_fd_b;
  t_socket* stick_fd_b = cpu_connected_pair(&peer_fd_b);
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
  socket_destroy(stick_fd_a);
  socket_destroy(peer_fd_a);
  socket_destroy(stick_fd_b);
  socket_destroy(peer_fd_b);
  log_destroy(cpu.logger);
}

/* ── request_write / receive_write_response ───────────────────────────── */

Test(cpu_memory, request_write_sends_the_offset_and_the_bytes)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_write_response_succeeds_on_write_done)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", peer_fd));
  cr_assert(receive_write_response(&cpu, stick));

  free(stick);
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_memory, receive_write_response_fails_when_the_stick_disconnected)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
  t_socket* km_fd;
  t_socket* km_peer_fd = cpu_connected_pair(&km_fd);
  t_cpu cpu = {.socket_kernel_memory = km_fd};
  cpu.logger = cpu_quiet_logger();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_fd;

  cr_assert(send_string(OP_CODE_ERROR, "disconnected", peer_fd));
  cr_assert_not(receive_write_response(&cpu, stick));

  free(stick);
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  socket_destroy(km_fd);
  socket_destroy(km_peer_fd);
  log_destroy(cpu.logger);
}

/* ── write_memory ──────────────────────────────────────────────────────── */

Test(cpu_memory, write_memory_writes_to_a_single_stick)
{
  t_socket* peer_fd;
  t_socket* stick_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(stick_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── failure paths ─────────────────────────────────────────────────────── */

/* A cpu wired to a Kernel Scheduler peer, a Kernel Memory peer and two
 * adjacent sticks: [0, 50) and [50, 100). */
typedef struct
{
  t_cpu cpu;
  t_socket* scheduler_peer;
  t_socket* km_peer;
  t_socket* stick_peer[2];
} t_memory_fixture;

static void memory_fixture_start(t_memory_fixture* fx)
{
  memset(fx, 0, sizeof(*fx));
  fx->cpu.logger = cpu_quiet_logger();
  fx->cpu.max_segment_size = 100;
  fx->cpu.socket_kernel_scheduler = cpu_connected_pair(&fx->scheduler_peer);
  fx->cpu.socket_kernel_memory = cpu_connected_pair(&fx->km_peer);
  fx->cpu.memory_sticks = list_create();
  for (int i = 0; i < 2; i++)
  {
    t_memory_stick_info* stick = cpu_make_stick(i * 50, 50);
    stick->socket_ms = cpu_connected_pair(&fx->stick_peer[i]);
    list_add(fx->cpu.memory_sticks, stick);
  }
}

static t_memory_stick_info* fixture_stick(t_memory_fixture* fx, int index)
{
  return list_get(fx->cpu.memory_sticks, index);
}

static void memory_fixture_end(t_memory_fixture* fx)
{
  for (int i = 0; i < 2; i++)
  {
    socket_destroy(fixture_stick(fx, i)->socket_ms);
    socket_destroy(fx->stick_peer[i]);
  }
  list_destroy_and_destroy_elements(fx->cpu.memory_sticks, free);
  socket_destroy(fx->cpu.socket_kernel_scheduler);
  socket_destroy(fx->scheduler_peer);
  socket_destroy(fx->cpu.socket_kernel_memory);
  socket_destroy(fx->km_peer);
  log_destroy(fx->cpu.logger);
}

/* Asserts Kernel Memory was told the stick went down (notify_bsod). */
static void assert_bsod_reported(t_memory_fixture* fx)
{
  cr_assert_eq(receive_op_code(fx->km_peer), OP_STICK_DISCONNECTED);
  free(receive_string(fx->km_peer));
}

Test(cpu_mmu, reports_an_error_when_the_segmentation_fault_cannot_be_sent)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);
  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));

  socket_shutdown(fx.cpu.socket_kernel_scheduler, SOCKET_SHUTDOWN_WRITE);
  cr_assert_eq(mmu(&fx.cpu, context, 30, 4, 7), INVALID_ADDRESS - 1);
  cr_assert_not(notify_seg_fault(&fx.cpu, 7));

  cpu_destroy_context(context);
  memory_fixture_end(&fx);
}

Test(cpu_memory, read_memory_fails_when_no_stick_owns_the_address)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  cr_assert_null(read_memory(&fx.cpu, 100, 4));

  memory_fixture_end(&fx);
}

Test(cpu_memory, read_memory_fails_and_reports_a_bsod_when_the_request_fails)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  socket_shutdown(fixture_stick(&fx, 0)->socket_ms, SOCKET_SHUTDOWN_WRITE);
  cr_assert_null(read_memory(&fx.cpu, 4, 4));
  assert_bsod_reported(&fx);

  memory_fixture_end(&fx);
}

Test(cpu_memory, read_memory_fails_and_reports_a_bsod_on_a_bad_response)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  cr_assert(
      send_string(OP_MEMORY_STICK_WRITE_DONE, "not a read", fx.stick_peer[0]));
  cr_assert_null(read_memory(&fx.cpu, 4, 4));
  assert_bsod_reported(&fx);

  memory_fixture_end(&fx);
}

Test(cpu_memory, write_memory_fails_when_no_stick_owns_the_address)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  char payload[4] = "abc";
  cr_assert_not(write_memory(&fx.cpu, 100, payload, sizeof(payload)));

  memory_fixture_end(&fx);
}

Test(cpu_memory, write_memory_spans_two_sticks)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", fx.stick_peer[0]));
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", fx.stick_peer[1]));

  /* 48..51: two bytes at the end of stick 0, two at the start of stick 1 */
  char payload[4] = {'A', 'B', 'C', 'D'};
  cr_assert(write_memory(&fx.cpu, 48, payload, sizeof(payload)));

  cr_assert_eq(receive_op_code(fx.stick_peer[0]), OP_MEMORY_STICK_WRITE);
  t_list* first = receive_packet(fx.stick_peer[0]);
  cr_assert_eq(*(int*)list_get(first, 0), 48);
  cr_assert_eq(memcmp(list_get(first, 1), "AB", 2), 0);
  cr_assert_eq(*(int*)list_get(first, 2), 2);
  list_destroy_and_destroy_elements(first, free);

  cr_assert_eq(receive_op_code(fx.stick_peer[1]), OP_MEMORY_STICK_WRITE);
  t_list* second = receive_packet(fx.stick_peer[1]);
  cr_assert_eq(*(int*)list_get(second, 0), 0);
  cr_assert_eq(memcmp(list_get(second, 1), "CD", 2), 0);
  cr_assert_eq(*(int*)list_get(second, 2), 2);
  list_destroy_and_destroy_elements(second, free);

  memory_fixture_end(&fx);
}

Test(cpu_memory, write_memory_fails_and_reports_a_bsod_when_the_request_fails)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  socket_shutdown(fixture_stick(&fx, 0)->socket_ms, SOCKET_SHUTDOWN_WRITE);
  char payload[4] = "abc";
  cr_assert_not(write_memory(&fx.cpu, 4, payload, sizeof(payload)));
  assert_bsod_reported(&fx);

  memory_fixture_end(&fx);
}

Test(cpu_memory, write_memory_fails_on_an_unexpected_response)
{
  t_memory_fixture fx;
  memory_fixture_start(&fx);

  cr_assert(
      send_string(OP_MEMORY_STICK_READ_DONE, "not a write", fx.stick_peer[0]));
  char payload[4] = "abc";
  cr_assert_not(write_memory(&fx.cpu, 4, payload, sizeof(payload)));

  memory_fixture_end(&fx);
}

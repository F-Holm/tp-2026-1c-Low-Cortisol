#include "kernel_memory/address_translation.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

/* ── translate_logical_address ─────────────────────────────────────────── */

Test(km_address_translation,
     translate_logical_address_computes_the_physical_offset)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(100, BEST, 0); /* max_segment=100 */
  list_add(memory->segments, km_make_segment(0, 1, 500, 100));

  /* logical address 50 -> segment 0, offset 50 */
  cr_assert_eq(translate_logical_address(1, 50, 4, memory, logger), 550);

  free_main_memory(memory);
  log_destroy(logger);
}

Test(km_address_translation,
     translate_logical_address_reports_a_missing_segment)
{
  t_log* logger = km_quiet_logger();
  t_main_memory* memory = init_main_memory(100, BEST, 0);

  cr_assert_eq(translate_logical_address(1, 50, 4, memory, logger), -1);

  free_main_memory(memory);
  log_destroy(logger);
}

/* ── find_stick ────────────────────────────────────────────────────────── */

Test(km_address_translation, find_stick_locates_the_owning_stick)
{
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  t_list* sticks = list_create();
  list_add(sticks, km_make_stick(100)); /* [0, 100) */
  list_add(sticks, km_make_stick(50));  /* [100, 150) */

  int offset;
  cr_assert_eq(find_stick(0, sticks, &mutex, &offset), 0);
  cr_assert_eq(offset, 0);
  cr_assert_eq(find_stick(99, sticks, &mutex, &offset), 0);
  cr_assert_eq(offset, 99);
  cr_assert_eq(find_stick(100, sticks, &mutex, &offset), 1);
  cr_assert_eq(offset, 0);
  cr_assert_eq(find_stick(149, sticks, &mutex, &offset), 1);
  cr_assert_eq(offset, 49);

  list_destroy_and_destroy_elements(sticks, free);
}

Test(km_address_translation, find_stick_reports_an_address_past_every_stick)
{
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  t_list* sticks = list_create();
  list_add(sticks, km_make_stick(100));

  int offset;
  cr_assert_eq(find_stick(100, sticks, &mutex, &offset), -1);

  list_destroy_and_destroy_elements(sticks, free);
}

/* ── read_from_sticks ──────────────────────────────────────────────────── */

Test(km_address_translation, read_from_sticks_reads_from_a_single_stick)
{
  int stick_server_fd;
  int stick_client_fd = km_connected_pair(&stick_server_fd);
  t_stick_data* stick = km_make_stick(1000);
  stick->socket_stick = stick_client_fd;
  t_list* sticks = list_create();
  list_add(sticks, stick);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, "abcd", 4, stick_server_fd));

  t_log* logger = km_quiet_logger();
  char* result = read_from_sticks(10, 4, sticks, &mutex, logger, -1);

  cr_assert_not_null(result);
  cr_assert_eq(memcmp(result, "abcd", 4), 0);

  free(result);
  list_destroy_and_destroy_elements(sticks, free);
  close(stick_client_fd);
  close(stick_server_fd);
  log_destroy(logger);
}

Test(km_address_translation, read_from_sticks_spans_two_sticks)
{
  int first_server_fd;
  int first_client_fd = km_connected_pair(&first_server_fd);
  int second_server_fd;
  int second_client_fd = km_connected_pair(&second_server_fd);

  t_stick_data* first = km_make_stick(4); /* [0, 4) */
  first->socket_stick = first_client_fd;
  t_stick_data* second = km_make_stick(4); /* [4, 8) */
  second->socket_stick = second_client_fd;
  t_list* sticks = list_create();
  list_add(sticks, first);
  list_add(sticks, second);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  /* physical_address 2, size 4 -> 2 bytes from the first stick, 2 from the
   * second */
  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, "ab", 2, first_server_fd));
  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, "cd", 2, second_server_fd));

  t_log* logger = km_quiet_logger();
  char* result = read_from_sticks(2, 4, sticks, &mutex, logger, -1);

  cr_assert_not_null(result);
  cr_assert_eq(memcmp(result, "abcd", 4), 0);

  free(result);
  list_destroy_and_destroy_elements(sticks, free);
  close(first_client_fd);
  close(first_server_fd);
  close(second_client_fd);
  close(second_server_fd);
  log_destroy(logger);
}

Test(km_address_translation, read_from_sticks_reports_an_address_with_no_stick)
{
  t_list* sticks = list_create();
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  t_log* logger = km_quiet_logger();

  cr_assert_null(read_from_sticks(0, 4, sticks, &mutex, logger, -1));

  list_destroy(sticks);
  log_destroy(logger);
}

Test(km_address_translation, read_from_sticks_reports_a_wrong_reply_opcode)
{
  int stick_server_fd;
  int stick_client_fd = km_connected_pair(&stick_server_fd);
  t_stick_data* stick = km_make_stick(1000);
  stick->socket_stick = stick_client_fd;
  t_list* sticks = list_create();
  list_add(sticks, stick);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  cr_assert(send_string(OP_ID_CPU, "not expected here", stick_server_fd));

  t_log* logger = km_quiet_logger();
  cr_assert_null(read_from_sticks(10, 4, sticks, &mutex, logger, -1));

  list_destroy_and_destroy_elements(sticks, free);
  close(stick_client_fd);
  close(stick_server_fd);
  log_destroy(logger);
}

/* ── write_to_sticks ───────────────────────────────────────────────────── */

Test(km_address_translation, write_to_sticks_writes_to_a_single_stick)
{
  int stick_server_fd;
  int stick_client_fd = km_connected_pair(&stick_server_fd);
  t_stick_data* stick = km_make_stick(1000);
  stick->socket_stick = stick_client_fd;
  t_list* sticks = list_create();
  list_add(sticks, stick);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", stick_server_fd));

  t_log* logger = km_quiet_logger();
  cr_assert(write_to_sticks(1, 10, 4, "data", sticks, &mutex, logger, -1));

  cr_assert_eq(receive_op_code(stick_server_fd), OP_MEMORY_STICK_WRITE);
  t_list* fields = receive_packet(stick_server_fd);
  cr_assert_eq(*(int*)list_get(fields, 0), 10); /* offset */
  cr_assert_eq(memcmp(list_get(fields, 1), "data", 4), 0);

  list_destroy_and_destroy_elements(fields, free);
  list_destroy_and_destroy_elements(sticks, free);
  close(stick_client_fd);
  close(stick_server_fd);
  log_destroy(logger);
}

Test(km_address_translation, write_to_sticks_spans_two_sticks)
{
  int first_server_fd;
  int first_client_fd = km_connected_pair(&first_server_fd);
  int second_server_fd;
  int second_client_fd = km_connected_pair(&second_server_fd);

  t_stick_data* first = km_make_stick(2); /* [0, 2) */
  first->socket_stick = first_client_fd;
  t_stick_data* second = km_make_stick(2); /* [2, 4) */
  second->socket_stick = second_client_fd;
  t_list* sticks = list_create();
  list_add(sticks, first);
  list_add(sticks, second);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", first_server_fd));
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", second_server_fd));

  t_log* logger = km_quiet_logger();
  cr_assert(write_to_sticks(1, 0, 4, "abcd", sticks, &mutex, logger, -1));

  cr_assert_eq(receive_op_code(first_server_fd), OP_MEMORY_STICK_WRITE);
  t_list* first_fields = receive_packet(first_server_fd);
  cr_assert_eq(memcmp(list_get(first_fields, 1), "ab", 2), 0);
  cr_assert_eq(receive_op_code(second_server_fd), OP_MEMORY_STICK_WRITE);
  t_list* second_fields = receive_packet(second_server_fd);
  cr_assert_eq(memcmp(list_get(second_fields, 1), "cd", 2), 0);

  list_destroy_and_destroy_elements(first_fields, free);
  list_destroy_and_destroy_elements(second_fields, free);
  list_destroy_and_destroy_elements(sticks, free);
  close(first_client_fd);
  close(first_server_fd);
  close(second_client_fd);
  close(second_server_fd);
  log_destroy(logger);
}

Test(km_address_translation, write_to_sticks_reports_running_out_of_sticks)
{
  t_stick_data* only = km_make_stick(2); /* [0, 2) */
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  only->socket_stick = client_fd;
  t_list* sticks = list_create();
  list_add(sticks, only);
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

  int scheduler_server_fd;
  int scheduler_client_fd = km_connected_pair(&scheduler_server_fd);
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", server_fd));

  t_log* logger = km_quiet_logger();
  /* 4 bytes requested but only one 2-byte stick exists */
  cr_assert_not(write_to_sticks(1, 0, 4, "abcd", sticks, &mutex, logger,
                                scheduler_client_fd));

  cr_assert_eq(receive_op_code(scheduler_server_fd), OP_MEMORY_CORRUPTED);
  free(receive_string(scheduler_server_fd));

  list_destroy_and_destroy_elements(sticks, free);
  close(client_fd);
  close(server_fd);
  close(scheduler_client_fd);
  close(scheduler_server_fd);
  log_destroy(logger);
}

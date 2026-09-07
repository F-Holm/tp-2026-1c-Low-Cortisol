#include "io/ioops.h"

#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

/* The handlers assume main() has already consumed the request op code, so every
 * test reads it back before handing the socket to the handler. */

/* ── run_sleep ─────────────────────────────────────────────────────────── */

Test(io_run_sleep, sleeps_then_answers_ok)
{
  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_sleep_request request = {.pid = 7, .blocked_time_ms = 5};
  send_buffer(OP_IO_SLEEP_REQUEST, &request, sizeof(request), scheduler_fd);

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_SLEEP_REQUEST);
  cr_assert(run_sleep(&io));

  cr_assert_eq(receive_op_code(scheduler_fd), OP_SLEEP_RESPONSE);
  char* answer = receive_string(scheduler_fd);
  cr_assert_str_eq(answer, "OK");

  free(answer);
  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

Test(io_run_sleep, reports_failure_when_the_answer_cannot_be_sent)
{
  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_sleep_request request = {.pid = 1, .blocked_time_ms = 0};
  send_buffer(OP_IO_SLEEP_REQUEST, &request, sizeof(request), scheduler_fd);
  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_SLEEP_REQUEST);

  shutdown(io.socket_io,
           SHUT_WR); /* the reply send() now fails deterministically */
  cr_assert_not(run_sleep(&io));

  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

/* ── run_stdout ────────────────────────────────────────────────────────── */

Test(io_run_stdout, prints_the_payload_and_answers_ok)
{
  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_stdout_request request = {.pid = 12, .bytes_to_write = 5};
  t_packet* packet = create_packet(OP_IO_STDOUT_REQUEST);
  packet_append(packet, &request, sizeof(request));
  packet_append_string(packet, "hello");
  send_packet(packet, scheduler_fd);
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_STDOUT_REQUEST);
  cr_assert(run_stdout(&io));

  cr_assert_eq(receive_op_code(scheduler_fd), OP_STDOUT_RESPONSE);
  char* answer = receive_string(scheduler_fd);
  cr_assert_str_eq(answer, "OK");

  free(answer);
  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

Test(io_run_stdout, fails_when_the_packet_carries_no_text)
{
  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_stdout_request request = {.pid = 3, .bytes_to_write = 0};
  t_packet* packet = create_packet(OP_IO_STDOUT_REQUEST);
  packet_append(packet, &request, sizeof(request)); /* no string appended */
  send_packet(packet, scheduler_fd);
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_STDOUT_REQUEST);
  cr_assert_not(run_stdout(&io));

  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

/* ── run_stdin ─────────────────────────────────────────────────────────── */

Test(io_run_stdin, returns_the_typed_line_to_the_scheduler,
     .init = cr_redirect_stdout)
{
  cr_redirect_stdin();

  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_stdin_request request = {.pid = 9, .bytes_to_read = 64};
  send_buffer(OP_IO_STDIN_REQUEST, &request, sizeof(request), scheduler_fd);

  FILE* keyboard = cr_get_redirected_stdin();
  fputs("typed answer\n", keyboard);
  fclose(keyboard);

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_STDIN_REQUEST);
  cr_assert(run_stdin(&io));

  cr_assert_eq(receive_op_code(scheduler_fd), OP_STDIN_RESPONSE);
  char* answer = receive_string(scheduler_fd);
  cr_assert_str_eq(answer, "typed answer");

  free(answer);
  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

Test(io_run_stdin, truncates_the_input_to_the_requested_size,
     .init = cr_redirect_stdout)
{
  cr_redirect_stdin();

  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_stdin_request request = {.pid = 4, .bytes_to_read = 4};
  send_buffer(OP_IO_STDIN_REQUEST, &request, sizeof(request), scheduler_fd);

  FILE* keyboard = cr_get_redirected_stdin();
  fputs("abcdefgh\n", keyboard);
  fclose(keyboard);

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_STDIN_REQUEST);
  cr_assert(run_stdin(&io));

  cr_assert_eq(receive_op_code(scheduler_fd), OP_STDIN_RESPONSE);
  char* answer = receive_string(scheduler_fd);
  cr_assert_str_eq(answer, "abc"); /* bytes_to_read - 1 characters kept */

  free(answer);
  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

Test(io_run_stdin, fails_on_end_of_input, .init = cr_redirect_stdout)
{
  cr_redirect_stdin();

  int scheduler_fd;
  int io_fd = io_connected_pair(&scheduler_fd);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.socket_io = io_fd;

  t_stdin_request request = {.pid = 2, .bytes_to_read = 16};
  send_buffer(OP_IO_STDIN_REQUEST, &request, sizeof(request), scheduler_fd);

  fclose(cr_get_redirected_stdin()); /* immediate EOF, nothing typed */

  cr_assert_eq(receive_op_code(io.socket_io), OP_IO_STDIN_REQUEST);
  cr_assert_not(run_stdin(&io));

  close(io_fd);
  close(scheduler_fd);
  log_destroy(io.logger);
}

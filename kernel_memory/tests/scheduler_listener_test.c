#include "kernel_memory/scheduler_listener.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/holes.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/segments.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

/* listen_scheduler's per-op handlers are static, so the only way to reach
 * them is through the real dispatch loop over a real socket, acting as the
 * fake Kernel Scheduler peer. Each test spins the loop up on its own thread,
 * drives it through one or more requests, then ends it (mostly via
 * OP_KERNEL_SCHEDULER_SHUTDOWN, which is itself one of the handlers under
 * test) and joins deterministically. */
typedef struct
{
  t_log* logger;
  int peer_fd;
  t_list* processes;
  pthread_mutex_t* processes_mutex;
  t_list* sticks;
  pthread_mutex_t* sticks_mutex;
  t_main_memory* memory;
  _Atomic(t_swap_data*)* swap_data_slot;
  /* Heap-allocated, not an inline struct member: start_fixture returns
   * t_listener_fixture by value, so an inline int's address would point at
   * start_fixture's own stack frame, dangling the moment it returns. */
  int* active_threads;
  pthread_mutex_t* active_threads_mutex;
  pthread_cond_t* active_threads_cond;
  t_scheduler_data* scheduler_data;
  pthread_t thread;
} t_listener_fixture;

static t_listener_fixture start_fixture(char* scripts_basepath)
{
  t_listener_fixture f = {0};
  f.logger = km_quiet_logger();
  int server_fd;
  f.peer_fd = km_connected_pair(&server_fd);

  f.processes = list_create();
  f.processes_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(f.processes_mutex, NULL);

  f.sticks = list_create();
  f.sticks_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(f.sticks_mutex, NULL);

  f.memory = init_main_memory(4096, AS_BEST, 0);

  f.swap_data_slot = malloc(sizeof(_Atomic(t_swap_data*)));
  atomic_init(f.swap_data_slot, NULL);

  f.active_threads = malloc(sizeof(int));
  *f.active_threads = 1;
  f.active_threads_mutex = malloc(sizeof(pthread_mutex_t));
  pthread_mutex_init(f.active_threads_mutex, NULL);
  f.active_threads_cond = malloc(sizeof(pthread_cond_t));
  pthread_cond_init(f.active_threads_cond, NULL);

  f.scheduler_data = init_scheduler_data(
      -1, server_fd, f.processes, scripts_basepath, f.processes_mutex, f.memory,
      f.sticks, f.sticks_mutex, f.swap_data_slot, f.logger, f.active_threads,
      f.active_threads_mutex, f.active_threads_cond);

  pthread_create(&f.thread, NULL, listen_scheduler, f.scheduler_data);
  return f;
}

/* Sends the raw shutdown op code (no body) -- listen_scheduler's
 * OP_KERNEL_SCHEDULER_SHUTDOWN handler ends the loop on just the op code. */
static void shutdown_and_join(t_listener_fixture* f)
{
  int op_code = OP_KERNEL_SCHEDULER_SHUTDOWN;
  cr_assert_eq(send(f->peer_fd, &op_code, sizeof(op_code), 0), sizeof(op_code));
  pthread_join(f->thread, NULL);
}

/* listen_scheduler already freed scheduler_data (and closed its socket) by
 * the time it returns -- only the fixture's own resources are ours to free
 * here. */
static void destroy_fixture(t_listener_fixture* f)
{
  close(f->peer_fd);
  list_destroy_and_destroy_elements(f->processes,
                                    (void (*)(void*))free_process);
  free(f->processes_mutex);
  list_destroy_and_destroy_elements(f->sticks, free);
  free(f->sticks_mutex);
  free_main_memory(f->memory);
  free(f->swap_data_slot);
  free(f->active_threads);
  free(f->active_threads_mutex);
  free(f->active_threads_cond);
  log_destroy(f->logger);
}

static t_process* add_process(t_listener_fixture* f, uint32_t pid)
{
  t_process* process = malloc(sizeof(t_process));
  process->pid = pid;
  process->instructions_path = NULL;
  process->instruction_count = 2;
  process->instructions = malloc(sizeof(char*) * 2);
  process->instructions[0] = strdup("NOOP");
  process->instructions[1] = strdup("EXIT");
  process->segments = list_create();
  memset(&process->registers, 0, sizeof(t_registers));
  list_add(f->processes, process);
  return process;
}

/* ── OP_KERNEL_SCHEDULER_SHUTDOWN / OP_CODE_ERROR ────────────────────────── */

Test(km_scheduler_listener, shutdown_op_ends_the_loop)
{
  t_listener_fixture f = start_fixture("/tmp");
  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, closing_the_socket_ends_the_loop)
{
  t_listener_fixture f = start_fixture("/tmp");
  close(f.peer_fd);
  pthread_join(f.thread, NULL);
  f.peer_fd = -1; /* already closed above; destroy_fixture closing it again is
                   * a harmless no-op (EBADF) */
  destroy_fixture(&f);
}

Test(km_scheduler_listener, an_unrecognized_op_code_ends_the_loop)
{
  t_listener_fixture f = start_fixture("/tmp");
  int op_code = 99999;
  cr_assert_eq(send(f.peer_fd, &op_code, sizeof(op_code), 0), sizeof(op_code));
  pthread_join(f.thread, NULL);
  destroy_fixture(&f);
}

/* ── OP_KERNEL_MEMORY_RUNNING ─────────────────────────────────────────────
 * (a connection-check ping the loop just drains and keeps running) */

Test(km_scheduler_listener, kernel_memory_running_ping_keeps_the_loop_alive)
{
  t_listener_fixture f = start_fixture("/tmp");
  cr_assert(send_string(OP_KERNEL_MEMORY_RUNNING, "ping", f.peer_fd));
  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_NEW_PROCESS ───────────────────────────────────────────────────────*/

Test(km_scheduler_listener, new_process_creates_and_registers_a_process)
{
  char dir[] = "/tmp/km_sched_listener_XXXXXX";
  cr_assert_not_null(mkdtemp(dir));
  char script[256];
  snprintf(script, sizeof(script), "%s/proc.prc", dir);
  FILE* file = fopen(script, "w");
  fputs("NOOP\nEXIT\n", file);
  fclose(file);

  t_listener_fixture f = start_fixture(dir);

  t_packet* packet = create_packet(OP_NEW_PROCESS);
  packet_append_string(packet, "proc.prc");
  uint32_t pid = 7;
  packet_append(packet, &pid, sizeof(pid));
  cr_assert(send_packet(packet, f.peer_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(f.peer_fd), OP_PROCESS_STARTED);
  free(receive_string(f.peer_fd));

  shutdown_and_join(&f);
  cr_assert_eq(list_size(f.processes), 1);
  t_process* created = list_get(f.processes, 0);
  cr_assert_eq(created->pid, 7);

  destroy_fixture(&f);
  unlink(script);
  rmdir(dir);
}

/* ── OP_CREATE_SEGMENT / OP_DELETE_SEGMENT ───────────────────────────────*/

Test(km_scheduler_listener, create_segment_reports_size_exceeded)
{
  t_listener_fixture f = start_fixture("/tmp");
  t_syscall_memory syscall = {.pid = 1, .segment_id = 0, .size = 999999};
  cr_assert(
      send_buffer(OP_CREATE_SEGMENT, &syscall, sizeof(syscall), f.peer_fd));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_SEGMENT_SIZE_EXCEEDED);
  free(receive_string(f.peer_fd));

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, create_segment_allocates_when_there_is_room)
{
  t_listener_fixture f = start_fixture("/tmp");
  list_add(f.memory->holes, km_make_hole(0, 1000));

  t_syscall_memory syscall = {.pid = 1, .segment_id = 0, .size = 100};
  cr_assert(
      send_buffer(OP_CREATE_SEGMENT, &syscall, sizeof(syscall), f.peer_fd));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_MEMORY_ALLOCATED);
  free(receive_string(f.peer_fd));
  cr_assert_eq(list_size(f.memory->segments), 1);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, delete_segment_frees_it_and_replies)
{
  t_listener_fixture f = start_fixture("/tmp");
  list_add(f.memory->segments, km_make_segment(0, 1, 0, 100));

  t_syscall_memory syscall = {.pid = 1, .segment_id = 0, .size = 100};
  cr_assert(
      send_buffer(OP_DELETE_SEGMENT, &syscall, sizeof(syscall), f.peer_fd));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_MEMORY_FREED);
  free(receive_string(f.peer_fd));
  cr_assert_eq(list_size(f.memory->segments), 0);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_END_PROCESS ───────────────────────────────────────────────────────*/

Test(km_scheduler_listener, end_process_removes_the_process_and_its_segments)
{
  t_listener_fixture f = start_fixture("/tmp");
  add_process(&f, 5);
  list_add(f.memory->segments, km_make_segment(0, 5, 0, 100));

  uint32_t pid = 5;
  cr_assert(send_buffer(OP_END_PROCESS, &pid, sizeof(pid), f.peer_fd));

  shutdown_and_join(&f);
  cr_assert_eq(list_size(f.processes), 0);
  cr_assert_eq(list_size(f.memory->segments), 0);

  destroy_fixture(&f);
}

Test(km_scheduler_listener, end_process_tolerates_an_unknown_pid)
{
  t_listener_fixture f = start_fixture("/tmp");
  uint32_t pid = 404;
  cr_assert(send_buffer(OP_END_PROCESS, &pid, sizeof(pid), f.peer_fd));
  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_REQUEST_FREE_MEMORY ───────────────────────────────────────────────*/

Test(km_scheduler_listener, request_free_memory_reports_the_free_space)
{
  t_listener_fixture f = start_fixture("/tmp");
  list_add(f.memory->holes, km_make_hole(0, 250));

  cr_assert(send_string(OP_REQUEST_FREE_MEMORY, "", f.peer_fd));
  cr_assert_eq(receive_op_code(f.peer_fd), OP_FREE_MEMORY);
  int a;
  int* size = receive_buffer(&a, f.peer_fd);
  cr_assert_eq(*size, 250);
  free(size);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_REQUEST_PROCESS_SIZE ──────────────────────────────────────────────*/

Test(km_scheduler_listener, request_process_size_sums_its_segments)
{
  t_listener_fixture f = start_fixture("/tmp");
  add_process(&f, 3);
  list_add(f.memory->segments, km_make_segment(0, 3, 0, 40));
  list_add(f.memory->segments, km_make_segment(1, 3, 40, 60));

  uint32_t pid = 3;
  cr_assert(send_buffer(OP_REQUEST_PROCESS_SIZE, &pid, sizeof(pid), f.peer_fd));
  cr_assert_eq(receive_op_code(f.peer_fd), OP_PROCESS_SIZE);
  int a;
  int* size = receive_buffer(&a, f.peer_fd);
  cr_assert_eq(*size, 100);
  free(size);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener,
     request_process_size_reports_zero_for_an_unknown_pid)
{
  t_listener_fixture f = start_fixture("/tmp");

  uint32_t pid = 404;
  cr_assert(send_buffer(OP_REQUEST_PROCESS_SIZE, &pid, sizeof(pid), f.peer_fd));
  cr_assert_eq(receive_op_code(f.peer_fd), OP_PROCESS_SIZE);
  int a;
  int* size = receive_buffer(&a, f.peer_fd);
  cr_assert_eq(*size, 0);
  free(size);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_SUSPEND_PROCESS / OP_RESUME_SUSPENDED_PROCESS ─────────────────────
 * suspend_process/resume_process are unit-tested directly (with a real swap
 * peer) in swap_test.c; here it's enough to prove the dispatch loop parses
 * the pid and reaches them -- the "swap not connected" guard is a
 * deterministic way to do that without standing up a swap peer. */

Test(km_scheduler_listener, suspend_process_reaches_suspend_process)
{
  t_listener_fixture f = start_fixture("/tmp");
  add_process(&f, 9);

  uint32_t pid = 9;
  cr_assert(send_buffer(OP_SUSPEND_PROCESS, &pid, sizeof(pid), f.peer_fd));
  cr_assert_eq(receive_op_code(f.peer_fd), OP_SUSPENSION_FAILED);
  free(receive_string(f.peer_fd));

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, resume_suspended_process_reaches_resume_process)
{
  t_listener_fixture f = start_fixture("/tmp");
  uint32_t pid = 9;
  cr_assert(
      send_buffer(OP_RESUME_SUSPENDED_PROCESS, &pid, sizeof(pid), f.peer_fd));
  cr_assert_eq(receive_op_code(f.peer_fd), OP_RESUME_SUSPENSION_FAILED);
  free(receive_string(f.peer_fd));

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

/* ── OP_IO_STDIN_REQUEST / OP_IO_STDOUT_REQUEST ───────────────────────────*/

Test(km_scheduler_listener, stdin_request_reports_a_segfault_with_no_segment)
{
  t_listener_fixture f = start_fixture("/tmp");
  t_stdin_request request = {
      .pid = 1, .bytes_to_read = 4, .logical_address = 0};
  t_packet* packet = create_packet(OP_IO_STDIN_REQUEST);
  packet_append(packet, &request, sizeof(request));
  packet_append_string(packet, "abcd");
  cr_assert(send_packet(packet, f.peer_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(f.peer_fd), OP_STDIN_RESPONSE);
  char* reply = receive_string(f.peer_fd);
  cr_assert_str_eq(reply, "Segmentation Fault");
  free(reply);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, stdin_request_writes_to_the_stick_and_replies)
{
  t_listener_fixture f = start_fixture("/tmp");
  list_add(f.memory->segments, km_make_segment(0, 1, 0, 100));
  int stick_server;
  int stick_client = km_connected_pair(&stick_server);
  t_stick_data* stick = km_make_stick(1000);
  stick->socket_stick = stick_client;
  list_add(f.sticks, stick);

  t_stdin_request request = {
      .pid = 1, .bytes_to_read = 4, .logical_address = 0};
  t_packet* packet = create_packet(OP_IO_STDIN_REQUEST);
  packet_append(packet, &request, sizeof(request));
  packet_append_string(packet, "abcd");
  cr_assert(send_packet(packet, f.peer_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(stick_server), OP_MEMORY_STICK_WRITE);
  free(receive_packet(stick_server));
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", stick_server));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_STDIN_RESPONSE);
  char* reply = receive_string(f.peer_fd);
  cr_assert_str_eq(reply, "Memory written");
  free(reply);

  shutdown_and_join(&f);
  close(stick_server);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, stdout_request_reports_a_segfault_with_no_segment)
{
  t_listener_fixture f = start_fixture("/tmp");
  t_stdout_request request = {
      .pid = 1, .bytes_to_write = 4, .logical_address = 0};
  cr_assert(
      send_buffer(OP_IO_STDOUT_REQUEST, &request, sizeof(request), f.peer_fd));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_STDOUT_RESPONSE);
  char* reply = receive_string(f.peer_fd);
  cr_assert_str_eq(reply, "Segmentation Fault");
  free(reply);

  shutdown_and_join(&f);
  destroy_fixture(&f);
}

Test(km_scheduler_listener, stdout_request_reads_from_the_stick_and_replies)
{
  t_listener_fixture f = start_fixture("/tmp");
  list_add(f.memory->segments, km_make_segment(0, 1, 0, 100));
  int stick_server;
  int stick_client = km_connected_pair(&stick_server);
  t_stick_data* stick = km_make_stick(1000);
  stick->socket_stick = stick_client;
  list_add(f.sticks, stick);

  t_stdout_request request = {
      .pid = 1, .bytes_to_write = 4, .logical_address = 0};
  cr_assert(
      send_buffer(OP_IO_STDOUT_REQUEST, &request, sizeof(request), f.peer_fd));

  cr_assert_eq(receive_op_code(stick_server), OP_MEMORY_STICK_READ);
  free(receive_packet(stick_server));
  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, "abcd", 4, stick_server));

  cr_assert_eq(receive_op_code(f.peer_fd), OP_STDOUT_RESPONSE);
  char* reply = receive_string(f.peer_fd);
  cr_assert_str_eq(reply, "abcd");
  free(reply);

  shutdown_and_join(&f);
  close(stick_server);
  destroy_fixture(&f);
}

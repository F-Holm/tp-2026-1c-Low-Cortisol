#include <criterion/criterion.h>
#include <stdatomic.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/connections/io.h"
#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/blocking_list.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

Test(ks_io_connection, succeeds_and_starts_a_worker_thread_for_a_valid_io_type)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, fds[0], queues, false, -1));
  cr_assert_eq(receive_handshake(fds[1]), MID_KERNEL_SCHEDULER);
  cr_assert_eq(io[E_STDIN].socket_io, fds[0]);

  close_io(io);
  close(fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_when_the_io_type_is_unknown)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "NOT_A_TYPE", fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert_not(handle_new_io(io, fds[0], queues, false, -1));

  close_io(io);
  close(fds[0]);
  close(fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_when_the_io_type_is_a_duplicate)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", fds[1]));
  int dup_fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, dup_fds), 0);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", dup_fds[1]));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, fds[0], queues, false, -1));
  cr_assert_not(handle_new_io(io, dup_fds[0], queues, false, -1));

  close_io(io);
  close(fds[1]);
  close(dup_fds[1]);
  free(queues);
  log_destroy(logger);
}

Test(ks_io_connection, fails_gracefully_on_a_dead_socket)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_io* io = create_io_structures();

  cr_assert_not(handle_new_io(io, -1, queues, false, -1));

  close_io(io);
  free(queues);
  log_destroy(logger);
}

/* ── enqueue_io_request ────────────────────────────────────────────────── */

static t_io make_io_stub(int io_type, bool priority_active)
{
  t_io io = {0};
  io.io_type = io_type;
  io.priority_active = priority_active;
  atomic_init(&(io.close_thread), false);
  pthread_cond_init(&(io.new_process), NULL);
  io.io_list = malloc(sizeof(t_io_list));
  io.io_list->io_list = list_create();
  pthread_mutex_init(&(io.io_list->io_list_mutex), NULL);
  return io;
}

static void destroy_io_stub(t_io* io)
{
  list_destroy_and_destroy_elements(io->io_list->io_list, free);
  pthread_mutex_destroy(&(io->io_list->io_list_mutex));
  free(io->io_list);
  pthread_cond_destroy(&(io->new_process));
}

Test(ks_io_connection, enqueue_io_request_is_rejected_after_close)
{
  t_io io = make_io_stub(E_STDIN, false);
  atomic_store(&(io.close_thread), true);
  t_pcb* pcb = create_pcb(EST_BLOCK, 0);

  cr_assert_not(
      enqueue_io_request(calloc(1, sizeof(t_stdin_request)), &io, pcb));
  cr_assert(list_is_empty(io.io_list->io_list));

  destroy_pcb(pcb);
  destroy_io_stub(&io);
}

Test(ks_io_connection, enqueue_io_request_appends_in_arrival_order_by_default)
{
  t_io io = make_io_stub(E_STDIN, false);
  t_pcb* low_priority = create_pcb(EST_BLOCK, 5);
  t_pcb* high_priority = create_pcb(EST_BLOCK, 1);

  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdin_request)), &io,
                               low_priority));
  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdin_request)), &io,
                               high_priority));

  cr_assert_eq(((t_stdin*)list_get(io.io_list->io_list, 0))->pcb, low_priority);
  cr_assert_eq(((t_stdin*)list_get(io.io_list->io_list, 1))->pcb,
               high_priority);

  destroy_pcb(low_priority);
  destroy_pcb(high_priority);
  destroy_io_stub(&io);
}

Test(ks_io_connection, enqueue_io_request_inserts_by_priority_when_enabled)
{
  t_io io = make_io_stub(E_STDIN, true);
  t_pcb* low_priority = create_pcb(EST_BLOCK, 5);
  t_pcb* high_priority = create_pcb(EST_BLOCK, 1);

  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdin_request)), &io,
                               low_priority));
  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdin_request)), &io,
                               high_priority));

  cr_assert_eq(((t_stdin*)list_get(io.io_list->io_list, 0))->pcb,
               high_priority);
  cr_assert_eq(((t_stdin*)list_get(io.io_list->io_list, 1))->pcb, low_priority);

  destroy_pcb(low_priority);
  destroy_pcb(high_priority);
  destroy_io_stub(&io);
}

Test(ks_io_connection, enqueue_io_request_inserts_stdout_by_priority)
{
  t_io io = make_io_stub(E_STDOUT, true);
  t_pcb* low_priority = create_pcb(EST_BLOCK, 5);
  t_pcb* high_priority = create_pcb(EST_BLOCK, 1);

  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdout_request)), &io,
                               low_priority));
  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdout_request)), &io,
                               high_priority));

  cr_assert_eq(((t_stdout*)list_get(io.io_list->io_list, 0))->pcb,
               high_priority);
  cr_assert_eq(((t_stdout*)list_get(io.io_list->io_list, 1))->pcb,
               low_priority);

  destroy_pcb(low_priority);
  destroy_pcb(high_priority);
  destroy_io_stub(&io);
}

Test(ks_io_connection, enqueue_io_request_inserts_sleep_by_priority)
{
  t_io io = make_io_stub(E_SLEEP, true);
  t_pcb* low_priority = create_pcb(EST_BLOCK, 5);
  t_pcb* high_priority = create_pcb(EST_BLOCK, 1);

  cr_assert(enqueue_io_request(calloc(1, sizeof(t_sleep_request)), &io,
                               low_priority));
  cr_assert(enqueue_io_request(calloc(1, sizeof(t_sleep_request)), &io,
                               high_priority));

  cr_assert_eq(((t_sleep*)list_get(io.io_list->io_list, 0))->pcb,
               high_priority);
  cr_assert_eq(((t_sleep*)list_get(io.io_list->io_list, 1))->pcb, low_priority);

  destroy_pcb(low_priority);
  destroy_pcb(high_priority);
  destroy_io_stub(&io);
}

/* ── close_io drains pending requests ─────────────────────────────────── */

Test(ks_io_connection, close_io_drains_and_unblocks_a_stuck_pending_request)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_IO_TYPE, "STDIN", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, client_fd, queues, false, -1));
  cr_assert_eq(receive_handshake(server_fd), MID_KERNEL_SCHEDULER);

  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));
  cr_assert(enqueue_io_request(calloc(1, sizeof(t_stdin_request)),
                               &(io[E_STDIN]), pcb));

  /* Give the worker thread a beat to pick it up and block on a reply that
   * will never come -- the fake "IO" peer above never answers. */
  usleep(50000);

  /* close_io shuts the socket down, which unblocks the worker's stuck read;
   * it then drains the still-in-flight request through free_request(),
   * which calls transition_unlock() on it. */
  close_io(io);

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(queues->block.list), 0);

  destroy_pcb(pcb);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

/* ── full round-trips through a real io_thread ────────────────────────── */

Test(ks_io_connection, sleep_round_trip_unblocks_the_process)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_IO_TYPE, "SLEEP", server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, client_fd, queues, false, -1));
  cr_assert_eq(receive_handshake(server_fd), MID_KERNEL_SCHEDULER);

  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));

  /* Queued ahead of time: communication_io_sleep() is a plain synchronous
   * send+receive against this same connection, so the reply just has to be
   * sitting on the wire whenever the worker thread gets to it. */
  cr_assert(send_string(OP_SLEEP_RESPONSE, "OK", server_fd));

  t_sleep_request* request = calloc(1, sizeof(t_sleep_request));
  request->pid = pcb->pid;
  cr_assert(enqueue_io_request(request, &(io[E_SLEEP]), pcb));

  usleep(50000); /* let the worker thread run the round-trip */

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(queues->block.list), 0);
  cr_assert(list_is_empty(io[E_SLEEP].io_list->io_list));

  destroy_pcb(pcb);
  close_io(io);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  log_destroy(logger);
}

Test(ks_io_connection, stdout_round_trip_prints_and_unblocks_the_process)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_IO_TYPE, "STDOUT", server_fd));

  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket->km_socket = km_client_fd;
  t_io* io = create_io_structures();

  cr_assert(handle_new_io(io, client_fd, queues, false, -1));
  cr_assert_eq(receive_handshake(server_fd), MID_KERNEL_SCHEDULER);

  t_pcb* pcb = create_pcb(EST_BLOCK, 0);
  transition_to_block(pcb, &(queues->block));

  /* io_stdout_f() first asks Kernel Memory for the text to print, then
   * relays it to the "IO" peer for printing -- both replies are queued
   * ahead of time, same reasoning as the sleep round-trip above. */
  cr_assert(send_string(OP_STDOUT_RESPONSE, "hello from memory", km_server_fd));
  cr_assert(send_string(OP_STDOUT_RESPONSE, "OK", server_fd));

  t_stdout_request* request = calloc(1, sizeof(t_stdout_request));
  request->pid = pcb->pid;
  cr_assert(enqueue_io_request(request, &(io[E_STDOUT]), pcb));

  usleep(50000);

  cr_assert_eq(pcb->state, EST_READY);
  cr_assert_eq(list_size(queues->block.list), 0);
  cr_assert(list_is_empty(io[E_STDOUT].io_list->io_list));

  destroy_pcb(pcb);
  close_io(io);
  ks_destroy_stub_queues_full(queues);
  close(server_fd);
  close(km_server_fd);
  log_destroy(logger);
}

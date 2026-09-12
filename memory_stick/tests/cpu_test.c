#include "memory_stick/cpu.h"

#include <criterion/criterion.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

/* Several of these spin up a peer thread -- cap every test so a stalled
 * handshake fails instead of hanging the run. */
TestSuite(ms_cpu_server, .timeout = 10.0);
TestSuite(ms_cpu_thread_data, .timeout = 10.0);
TestSuite(ms_cpu_iterator, .timeout = 10.0);
TestSuite(ms_handshake_cpu, .timeout = 10.0);
TestSuite(ms_receive_cpu_id, .timeout = 10.0);
TestSuite(ms_handle_new_cpu, .timeout = 10.0);
TestSuite(ms_close_listen_thread, .timeout = 10.0);
TestSuite(ms_handle_cpu_client, .timeout = 10.0);
TestSuite(ms_cpu_listen_thread, .timeout = 10.0);

/* ── create_server_cpu / get_cpu_port ──────────────────────────────────── */

Test(ms_cpu_server, create_server_cpu_opens_a_listening_socket_with_a_port)
{
  t_log* logger = ms_quiet_logger();

  int server = create_server_cpu(logger);
  cr_assert_gt(server, 0);
  cr_assert_gt(get_cpu_port(server), 0);

  close(server);
  log_destroy(logger);
}

/* ── create_cpu_thread_data ────────────────────────────────────────────── */

Test(ms_cpu_thread_data, keeps_every_field)
{
  t_list list;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  t_ms ms;

  t_cpu_thread* data = create_cpu_thread_data(42, &list, &mutex, &cond, &ms);
  cr_assert_eq(data->socket_cpu, 42);
  cr_assert_eq(data->socket_list, &list);
  cr_assert_eq(data->socket_list_mutex, &mutex);
  cr_assert_eq(data->listen_done_cond, &cond);
  cr_assert_eq(data->ms, &ms);
  free(data);
}

/* ── iterator_shutdown ─────────────────────────────────────────────────── */

Test(ms_cpu_iterator, iterator_shutdown_stops_the_socket)
{
  int fds[2];
  cr_assert_eq(socketpair(AF_UNIX, SOCK_STREAM, 0, fds), 0);

  iterator_shutdown(&fds[0]);

  char byte;
  cr_assert_eq(read(fds[0], &byte, 1), 0); /* shutdown -> EOF */

  close(fds[0]);
  close(fds[1]);
}

/* ── handshake_cpu ─────────────────────────────────────────────────────── */

struct cpu_side
{
  int fd;
  bool ok;
};

static void* cpu_handshake_thread(void* arg)
{
  struct cpu_side* side = arg;
  send_handshake(MID_CPU, side->fd);
  side->ok = receive_handshake(side->fd) == MID_MEMORY_STICK;
  return NULL;
}

Test(ms_handshake_cpu, exchanges_the_handshake_with_a_cpu)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  struct cpu_side side = {.fd = cpu_fd};
  pthread_t thread;
  pthread_create(&thread, NULL, cpu_handshake_thread, &side);

  cr_assert(handshake_cpu(stick_fd, logger));

  pthread_join(thread, NULL);
  cr_assert(side.ok);

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

Test(ms_handshake_cpu, fails_when_the_peer_is_not_a_cpu)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_handshake(MID_KERNEL_SCHEDULER, cpu_fd); /* not MID_CPU */
  cr_assert_not(handshake_cpu(stick_fd, logger));

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

/* ── receive_cpu_id ────────────────────────────────────────────────────── */

Test(ms_receive_cpu_id, reads_the_id_after_the_op_code)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_string(OP_ID_CPU, "CPU-3", cpu_fd);

  char* id = receive_cpu_id(stick_fd, logger);
  cr_assert_not_null(id);
  cr_assert_str_eq(id, "CPU-3");
  free(id);

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

Test(ms_receive_cpu_id, rejects_a_wrong_op_code)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();

  send_string(OP_HANDSHAKE, "CPU-3", cpu_fd);
  cr_assert_null(receive_cpu_id(stick_fd, logger));

  close(cpu_fd);
  close(stick_fd);
  log_destroy(logger);
}

/* ── handle_new_cpu ────────────────────────────────────────────────────── */

Test(ms_handle_new_cpu, accepts_a_cpu_and_spawns_its_client_thread)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();
  t_ms* ms = ms_make(64);

  t_listen_thread listen_thread = {
      .cpu_listen_socket = -1, .logger = logger, .ms = ms};

  t_list* socket_list = list_create();
  pthread_mutex_t socket_list_mutex;
  pthread_cond_t listen_done_cond;
  pthread_mutex_init(&socket_list_mutex, NULL);
  pthread_cond_init(&listen_done_cond, NULL);

  /* The handshake + id are sent up front so they're already sitting in the
   * socket buffer by the time handle_new_cpu does its synchronous,
   * blocking read of them. */
  cr_assert(send_handshake(MID_CPU, cpu_fd));
  cr_assert(send_string(OP_ID_CPU, "CPU-1", cpu_fd));

  cr_assert(handle_new_cpu(&listen_thread, stick_fd, socket_list,
                           &socket_list_mutex, &listen_done_cond, ms));
  cr_assert_eq(list_size(socket_list), 1);
  cr_assert_eq(*(int*)list_get(socket_list, 0), stick_fd);

  /* handle_new_cpu spawned a real, detached handle_cpu_client thread over
   * stick_fd. Shut its peer down so its next receive_op_code() fails,
   * driving it into close_cpu_thread(), which empties socket_list and
   * signals listen_done_cond -- wait for that deterministically before
   * cleaning up, exactly like close_listen_thread does. */
  shutdown(cpu_fd, SHUT_RDWR);

  pthread_mutex_lock(&socket_list_mutex);
  while (!list_is_empty(socket_list))
    pthread_cond_wait(&listen_done_cond, &socket_list_mutex);
  pthread_mutex_unlock(&socket_list_mutex);

  close(cpu_fd);
  list_destroy(socket_list);
  pthread_cond_destroy(&listen_done_cond);
  pthread_mutex_destroy(&socket_list_mutex);
  ms_destroy(ms);
  log_destroy(logger);
}

Test(ms_handle_new_cpu,
     returns_false_and_leaves_the_list_empty_when_the_handshake_fails)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();
  t_ms* ms = ms_make(64);

  t_listen_thread listen_thread = {
      .cpu_listen_socket = -1, .logger = logger, .ms = ms};

  t_list* socket_list = list_create();
  pthread_mutex_t socket_list_mutex;
  pthread_cond_t listen_done_cond;
  pthread_mutex_init(&socket_list_mutex, NULL);
  pthread_cond_init(&listen_done_cond, NULL);

  send_handshake(MID_KERNEL_SCHEDULER, cpu_fd); /* not MID_CPU */

  cr_assert_not(handle_new_cpu(&listen_thread, stick_fd, socket_list,
                               &socket_list_mutex, &listen_done_cond, ms));
  cr_assert(list_is_empty(socket_list));

  close(cpu_fd);
  close(stick_fd);
  list_destroy(socket_list);
  pthread_cond_destroy(&listen_done_cond);
  pthread_mutex_destroy(&socket_list_mutex);
  ms_destroy(ms);
  log_destroy(logger);
}

/* ── close_listen_thread ───────────────────────────────────────────────── */

Test(ms_close_listen_thread, waits_for_the_client_thread_then_frees_everything)
{
  int stick_fd;
  int cpu_fd = ms_connected_pair(&stick_fd);
  t_log* logger = ms_quiet_logger();
  t_ms* ms = ms_make(64);

  t_listen_thread* listen_thread = malloc(sizeof(t_listen_thread));
  listen_thread->cpu_listen_socket = -1;
  listen_thread->logger = logger;
  listen_thread->ms = ms;

  t_list* socket_list = list_create();
  pthread_mutex_t socket_list_mutex;
  pthread_cond_t listen_done_cond;
  pthread_mutex_init(&socket_list_mutex, NULL);
  pthread_cond_init(&listen_done_cond, NULL);

  cr_assert(send_handshake(MID_CPU, cpu_fd));
  cr_assert(send_string(OP_ID_CPU, "CPU-2", cpu_fd));

  /* Get a real handle_cpu_client thread running with a real fd registered
   * in socket_list, matching what cpu_listen_thread would have done. */
  cr_assert(handle_new_cpu(listen_thread, stick_fd, socket_list,
                           &socket_list_mutex, &listen_done_cond, ms));

  /* close_listen_thread() shuts every socket in the list down itself (via
   * iterator_shutdown), which drives the real client thread into
   * close_cpu_thread() and empties the list -- so this call returns on its
   * own once that happens. It also frees socket_list, destroys the
   * mutex/cond and frees listen_thread, so there is nothing left for this
   * test to clean up for those. */
  close_listen_thread(socket_list, &socket_list_mutex, &listen_done_cond,
                      listen_thread);

  close(cpu_fd);
  ms_destroy(ms);
  log_destroy(logger);
}

/* ── handle_cpu_client ─────────────────────────────────────────────────── */

typedef struct
{
  int cpu_fd;   /* client side, driven by the test as the "CPU" peer */
  int stick_fd; /* server side, owned by the spawned thread */
  t_ms* ms;
  t_list* socket_list;
  pthread_mutex_t socket_list_mutex;
  pthread_cond_t listen_done_cond;
  pthread_t thread;
} t_handle_cpu_client_fixture;

static void handle_cpu_client_fixture_start(t_handle_cpu_client_fixture* fx,
                                            int memory_size)
{
  fx->cpu_fd = ms_connected_pair(&fx->stick_fd);
  fx->ms = ms_make(memory_size);
  fx->socket_list = list_create();
  pthread_mutex_init(&fx->socket_list_mutex, NULL);
  pthread_cond_init(&fx->listen_done_cond, NULL);

  /* Mirrors what handle_new_cpu does before spawning the thread: register
   * the fd in socket_list first. */
  t_cpu_thread* cpu_thread = create_cpu_thread_data(
      fx->stick_fd, fx->socket_list, &fx->socket_list_mutex,
      &fx->listen_done_cond, fx->ms);
  list_add(fx->socket_list, &(cpu_thread->socket_cpu));

  cr_assert_eq(pthread_create(&fx->thread, NULL, handle_cpu_client, cpu_thread),
               0);
}

/* Sends an op code handle_cpu_client's switch doesn't know about, which
 * drives it into close_cpu_thread() (closing stick_fd and emptying
 * socket_list) and out of its loop -- the deterministic way to end the
 * thread before joining it. */
static void handle_cpu_client_fixture_terminate(t_handle_cpu_client_fixture* fx)
{
  /* Just the raw op code, matching exactly what receive_op_code() reads --
   * a full send_packet() would leave its size-field unread on the wire,
   * which turns the following close() into a TCP reset instead of a clean
   * FIN, breaking anything that expects a plain EOF from the other end. */
  int op_code = OP_HANDSHAKE;
  cr_assert_eq(send(fx->cpu_fd, &op_code, sizeof(op_code), 0), sizeof(op_code));

  pthread_mutex_lock(&fx->socket_list_mutex);
  while (!list_is_empty(fx->socket_list))
    pthread_cond_wait(&fx->listen_done_cond, &fx->socket_list_mutex);
  pthread_mutex_unlock(&fx->socket_list_mutex);
}

static void handle_cpu_client_fixture_end(t_handle_cpu_client_fixture* fx)
{
  pthread_join(fx->thread, NULL);
  close(fx->cpu_fd);
  list_destroy(fx->socket_list);
  pthread_cond_destroy(&fx->listen_done_cond);
  pthread_mutex_destroy(&fx->socket_list_mutex);
  ms_destroy(fx->ms);
}

Test(ms_handle_cpu_client, answers_a_valid_read_request)
{
  t_handle_cpu_client_fixture fx;
  handle_cpu_client_fixture_start(&fx, 16);
  memcpy(fx.ms->memory, "ABCDEFGH", 8);

  int start_position = 0;
  int byte_count = 8;
  t_packet* packet = create_packet(OP_MEMORY_STICK_READ);
  packet_append(packet, &start_position, sizeof(int));
  packet_append(packet, &byte_count, sizeof(int));
  cr_assert(send_packet(packet, fx.cpu_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(fx.cpu_fd), OP_MEMORY_STICK_READ_DONE);
  int size;
  void* data = receive_buffer(&size, fx.cpu_fd);
  cr_assert_eq(size, 8);
  cr_assert_eq(memcmp(data, "ABCDEFGH", 8), 0);
  free(data);

  handle_cpu_client_fixture_terminate(&fx);
  handle_cpu_client_fixture_end(&fx);
}

Test(ms_handle_cpu_client, answers_a_valid_write_request)
{
  t_handle_cpu_client_fixture fx;
  handle_cpu_client_fixture_start(&fx, 16);

  int start_position = 0;
  char bytes_to_write[8] = "WRITTEN";
  int byte_count = sizeof(bytes_to_write);
  t_packet* packet = create_packet(OP_MEMORY_STICK_WRITE);
  packet_append(packet, &start_position, sizeof(int));
  packet_append(packet, bytes_to_write, byte_count);
  packet_append(packet, &byte_count, sizeof(int));
  cr_assert(send_packet(packet, fx.cpu_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(fx.cpu_fd), OP_MEMORY_STICK_WRITE_DONE);
  char* response = receive_string(fx.cpu_fd);
  cr_assert_str_eq(response, "Write successful");
  free(response);
  cr_assert_eq(memcmp(fx.ms->memory, bytes_to_write, byte_count), 0);

  handle_cpu_client_fixture_terminate(&fx);
  handle_cpu_client_fixture_end(&fx);
}

Test(ms_handle_cpu_client, survives_a_malformed_read_request_and_keeps_serving)
{
  t_handle_cpu_client_fixture fx;
  handle_cpu_client_fixture_start(&fx, 16);

  /* Only one field instead of the expected (start_position, byte_count). */
  int only_one_field = 0;
  t_packet* bad = create_packet(OP_MEMORY_STICK_READ);
  packet_append(bad, &only_one_field, sizeof(int));
  cr_assert(send_packet(bad, fx.cpu_fd));
  destroy_packet(bad);

  /* Prove the thread logged the error and kept looping instead of dying,
   * by sending a normal request right after and getting a real answer. */
  memcpy(fx.ms->memory, "OK!", 4);
  int start_position = 0;
  int byte_count = 4;
  t_packet* good = create_packet(OP_MEMORY_STICK_READ);
  packet_append(good, &start_position, sizeof(int));
  packet_append(good, &byte_count, sizeof(int));
  cr_assert(send_packet(good, fx.cpu_fd));
  destroy_packet(good);

  cr_assert_eq(receive_op_code(fx.cpu_fd), OP_MEMORY_STICK_READ_DONE);
  int size;
  void* data = receive_buffer(&size, fx.cpu_fd);
  cr_assert_eq(size, 4);
  cr_assert_eq(memcmp(data, "OK!", 4), 0);
  free(data);

  handle_cpu_client_fixture_terminate(&fx);
  handle_cpu_client_fixture_end(&fx);
}

Test(ms_handle_cpu_client, survives_a_malformed_write_request_and_keeps_serving)
{
  t_handle_cpu_client_fixture fx;
  handle_cpu_client_fixture_start(&fx, 16);

  /* Only two fields instead of the expected (start_position,
   * bytes_to_write, byte_count). */
  int start_position = 0;
  char bytes_to_write[4] = "hi";
  t_packet* bad = create_packet(OP_MEMORY_STICK_WRITE);
  packet_append(bad, &start_position, sizeof(int));
  packet_append(bad, bytes_to_write, sizeof(bytes_to_write));
  cr_assert(send_packet(bad, fx.cpu_fd));
  destroy_packet(bad);

  /* Prove the thread kept looping by sending a normal write right after. */
  char good_bytes[4] = "bye";
  int byte_count = sizeof(good_bytes);
  t_packet* good = create_packet(OP_MEMORY_STICK_WRITE);
  packet_append(good, &start_position, sizeof(int));
  packet_append(good, good_bytes, byte_count);
  packet_append(good, &byte_count, sizeof(int));
  cr_assert(send_packet(good, fx.cpu_fd));
  destroy_packet(good);

  cr_assert_eq(receive_op_code(fx.cpu_fd), OP_MEMORY_STICK_WRITE_DONE);
  char* response = receive_string(fx.cpu_fd);
  cr_assert_str_eq(response, "Write successful");
  free(response);
  cr_assert_eq(memcmp(fx.ms->memory, good_bytes, byte_count), 0);

  handle_cpu_client_fixture_terminate(&fx);
  handle_cpu_client_fixture_end(&fx);
}

Test(ms_handle_cpu_client, exits_via_close_cpu_thread_on_an_unknown_op_code)
{
  t_handle_cpu_client_fixture fx;
  handle_cpu_client_fixture_start(&fx, 16);

  handle_cpu_client_fixture_terminate(&fx);

  /* close_cpu_thread() already closed stick_fd -- observable from the
   * client side as EOF. */
  char byte;
  cr_assert_eq(read(fx.cpu_fd, &byte, 1), 0);

  handle_cpu_client_fixture_end(&fx);
}

/* ── cpu_listen_thread ─────────────────────────────────────────────────── */

Test(ms_cpu_listen_thread, accepts_a_cpu_and_serves_a_request_end_to_end)
{
  t_log* logger = ms_quiet_logger();
  t_ms* ms = ms_make(16);

  int listen_socket = create_server_cpu(logger);
  cr_assert_gt(listen_socket, 0);
  uint16_t port = get_cpu_port(listen_socket);

  t_listen_thread* listen_thread = malloc(sizeof(t_listen_thread));
  listen_thread->cpu_listen_socket = listen_socket;
  listen_thread->logger = logger;
  listen_thread->ms = ms;

  pthread_t thread;
  cr_assert_eq(pthread_create(&thread, NULL, cpu_listen_thread, listen_thread),
               0);

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%hu", port);
  int cpu_fd = create_connection("127.0.0.1", port_str);
  cr_assert_geq(cpu_fd, 0);

  cr_assert(send_handshake(MID_CPU, cpu_fd));
  cr_assert_eq(receive_handshake(cpu_fd), MID_MEMORY_STICK);
  cr_assert(send_string(OP_ID_CPU, "CPU-listen", cpu_fd));

  memcpy(ms->memory, "ABCDEFGH", 8);
  int start_position = 0;
  int byte_count = 8;
  t_packet* packet = create_packet(OP_MEMORY_STICK_READ);
  packet_append(packet, &start_position, sizeof(int));
  packet_append(packet, &byte_count, sizeof(int));
  cr_assert(send_packet(packet, cpu_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(cpu_fd), OP_MEMORY_STICK_READ_DONE);
  int size;
  void* data = receive_buffer(&size, cpu_fd);
  cr_assert_eq(size, 8);
  cr_assert_eq(memcmp(data, "ABCDEFGH", 8), 0);
  free(data);

  /* Stop everything cleanly, the same way close_module() does in
   * production: closing the client unwinds its own handle_cpu_client
   * thread, then shutting the listening socket down unblocks accept(),
   * which breaks cpu_listen_thread out of its loop and into
   * close_listen_thread(). */
  close(cpu_fd);
  shutdown(listen_socket, SHUT_RDWR);

  pthread_join(thread, NULL);

  close(listen_socket);
  ms_destroy(ms);
  log_destroy(logger);
}

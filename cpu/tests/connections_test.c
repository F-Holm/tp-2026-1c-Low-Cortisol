#include "cpu/connections.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>
#include <unistd.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

Test(cpu_connections, compute_offset_sums_every_stick_size)
{
  t_list* sticks = list_create();
  cr_assert_eq(compute_offset(sticks), 0);

  list_add(sticks, cpu_make_stick(0, 100));
  list_add(sticks, cpu_make_stick(100, 250));
  list_add(sticks, cpu_make_stick(350, 40));

  cr_assert_eq(compute_offset(sticks), 390);

  list_destroy_and_destroy_elements(sticks, free);
}

/* ── handshake_memory_stick ────────────────────────────────────────────── */

struct handshake_stub_args
{
  t_socket* socket_fd;
  int announce_as;
};

static int handshake_stub_thread(void* arg)
{
  struct handshake_stub_args* args = arg;
  receive_handshake(args->socket_fd);
  send_handshake(args->announce_as, args->socket_fd);
  return 0;
}

Test(cpu_connections, handshake_memory_stick_succeeds_when_the_peer_agrees)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();

  struct handshake_stub_args args = {server_fd, MID_MEMORY_STICK};
  thrd_t stub;
  thrd_create(&stub, handshake_stub_thread, &args);

  cr_assert(handshake_memory_stick(&cpu, client_fd));

  thrd_join(stub, NULL);
  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_connections, handshake_memory_stick_fails_when_the_peer_is_not_a_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();

  struct handshake_stub_args args = {server_fd, MID_CPU};
  thrd_t stub;
  thrd_create(&stub, handshake_stub_thread, &args);

  cr_assert_not(handshake_memory_stick(&cpu, client_fd));

  thrd_join(stub, NULL);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

/* ── notify_bsod ───────────────────────────────────────────────────────── */

Test(cpu_connections, notify_bsod_tells_kernel_memory_the_stick_disconnected)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  notify_bsod(&cpu);

  cr_assert_eq(receive_op_code(server_fd), OP_STICK_DISCONNECTED);
  free(receive_string(server_fd));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

/* ── connect_to_kernel_scheduler / connect_to_kernel_memory ───────────── */

struct accept_and_handshake_args
{
  t_socket* listen_fd;
  int announce_as;
  t_socket* accepted_fd;
};

static int accept_and_handshake_thread(void* arg)
{
  struct accept_and_handshake_args* args = arg;
  args->accepted_fd = socket_accept(args->listen_fd, false);
  struct handshake_stub_args handshake_args = {args->accepted_fd,
                                               args->announce_as};
  handshake_stub_thread(&handshake_args);
  return 0;
}

Test(cpu_connections, connect_to_kernel_scheduler_succeeds_when_the_peer_agrees)
{
  char port[16];
  t_socket* listen_fd = cpu_listen_ephemeral(port, sizeof(port));

  char config_body[256];
  snprintf(config_body, sizeof(config_body),
           "KERNEL_SCHEDULER_IP=127.0.0.1\nKERNEL_SCHEDULER_PORT=%s\n", port);
  char* config_path = cpu_write_temp_config(config_body);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.config = config_create(config_path);

  struct accept_and_handshake_args args = {listen_fd, MID_KERNEL_SCHEDULER,
                                           NULL};
  thrd_t stub;
  thrd_create(&stub, accept_and_handshake_thread, &args);

  cr_assert(connect_to_kernel_scheduler(&cpu));
  cr_assert_not_null(cpu.socket_kernel_scheduler);

  thrd_join(stub, NULL);
  socket_destroy(cpu.socket_kernel_scheduler);
  socket_destroy(args.accepted_fd);
  socket_destroy(listen_fd);
  config_destroy(cpu.config);
  unlink(config_path);
  free(config_path);
  log_destroy(cpu.logger);
}

Test(cpu_connections,
     connect_to_kernel_scheduler_fails_when_the_peer_is_not_the_scheduler)
{
  char port[16];
  t_socket* listen_fd = cpu_listen_ephemeral(port, sizeof(port));

  char config_body[256];
  snprintf(config_body, sizeof(config_body),
           "KERNEL_SCHEDULER_IP=127.0.0.1\nKERNEL_SCHEDULER_PORT=%s\n", port);
  char* config_path = cpu_write_temp_config(config_body);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.config = config_create(config_path);

  struct accept_and_handshake_args args = {listen_fd, MID_CPU, NULL};
  thrd_t stub;
  thrd_create(&stub, accept_and_handshake_thread, &args);

  cr_assert_not(connect_to_kernel_scheduler(&cpu));

  thrd_join(stub, NULL);
  socket_destroy(args.accepted_fd);
  socket_destroy(listen_fd);
  config_destroy(cpu.config);
  unlink(config_path);
  free(config_path);
  log_destroy(cpu.logger);
}

Test(cpu_connections, connect_to_kernel_memory_succeeds_when_the_peer_agrees)
{
  char port[16];
  t_socket* listen_fd = cpu_listen_ephemeral(port, sizeof(port));

  char config_body[256];
  snprintf(config_body, sizeof(config_body),
           "KERNEL_MEMORY_IP=127.0.0.1\nKERNEL_MEMORY_PORT=%s\n", port);
  char* config_path = cpu_write_temp_config(config_body);

  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.config = config_create(config_path);

  struct accept_and_handshake_args args = {listen_fd, MID_KERNEL_MEMORY, NULL};
  thrd_t stub;
  thrd_create(&stub, accept_and_handshake_thread, &args);

  cr_assert(connect_to_kernel_memory(&cpu));
  cr_assert_not_null(cpu.socket_kernel_memory);

  thrd_join(stub, NULL);
  socket_destroy(cpu.socket_kernel_memory);
  socket_destroy(args.accepted_fd);
  socket_destroy(listen_fd);
  config_destroy(cpu.config);
  unlink(config_path);
  free(config_path);
  log_destroy(cpu.logger);
}

/* ── connect_memory_stick ──────────────────────────────────────────────── */

struct stick_stub_args
{
  t_socket* listen_fd;
  t_socket* accepted_fd;
};

static int stick_stub_thread(void* arg)
{
  struct stick_stub_args* args = arg;
  args->accepted_fd = socket_accept(args->listen_fd, false);
  receive_handshake(args->accepted_fd);
  send_handshake(MID_MEMORY_STICK, args->accepted_fd);
  free(
      receive_string(args->accepted_fd)); /* the CPU's OP_ID_CPU announcement */
  return 0;
}

Test(cpu_connections, connect_memory_stick_registers_the_new_stick)
{
  /* cpu->socket_kernel_memory: a fake "Kernel Memory" that hands the CPU a
   * (ip, port, size) packet pointing at a fake Memory Stick. */
  t_socket* km_server_fd;
  t_socket* km_client_fd = cpu_connected_pair(&km_server_fd);

  char stick_port[16];
  t_socket* stick_listen_fd =
      cpu_listen_ephemeral(stick_port, sizeof(stick_port));

  t_packet* packet = create_packet(OP_PACKET);
  packet_append_string(packet, "127.0.0.1");
  packet_append_string(packet, stick_port);
  int stick_size = 1024;
  packet_append(packet, &stick_size, sizeof(int));
  send_packet(packet, km_server_fd);
  destroy_packet(packet);

  t_cpu cpu = {.socket_kernel_memory = km_client_fd, .id = "3"};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();

  struct stick_stub_args args = {stick_listen_fd, NULL};
  thrd_t stub;
  thrd_create(&stub, stick_stub_thread, &args);

  /* connect_memory_stick is normally invoked by listen_kernel_memory right
   * after it reads the OP_PACKET op code that announces this packet; do the
   * same here, since receive_packet() expects the op code already gone. */
  cr_assert_eq(receive_op_code(km_client_fd), OP_PACKET);
  cr_assert(connect_memory_stick(&cpu));
  cr_assert_eq(list_size(cpu.memory_sticks), 1);
  t_memory_stick_info* stick = list_get(cpu.memory_sticks, 0);
  cr_assert_eq(stick->size, 1024);
  cr_assert_eq(stick->offset, 0);

  thrd_join(stub, NULL);
  socket_destroy(stick->socket_ms);
  socket_destroy(args.accepted_fd);
  socket_destroy(stick_listen_fd);
  socket_destroy(km_client_fd);
  socket_destroy(km_server_fd);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  log_destroy(cpu.logger);
}

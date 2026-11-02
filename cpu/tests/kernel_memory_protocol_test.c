#include "cpu/kernel_memory_protocol.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

static t_list* stick_packet(const char* ip, const char* port, int size)
{
  t_list* packet = list_create();
  list_add(packet, strdup(ip));
  list_add(packet, strdup(port));
  int* size_box = malloc(sizeof(int));
  *size_box = size;
  list_add(packet, size_box);
  return packet;
}

Test(cpu_parse_stick_packet, extracts_ip_port_and_size)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();

  char ip[16] = {0};
  char port[6] = {0};
  uint32_t size = 0;
  /* parse_stick_packet takes ownership of the packet */
  cr_assert(parse_stick_packet(&cpu, stick_packet("127.0.0.1", "5003", 1024),
                               ip, port, &size));
  cr_assert_str_eq(ip, "127.0.0.1");
  cr_assert_str_eq(port, "5003");
  cr_assert_eq(size, 1024);

  log_destroy(cpu.logger);
}

Test(cpu_parse_stick_packet, rejects_a_packet_without_three_fields)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();

  t_list* packet = list_create();
  list_add(packet, strdup("127.0.0.1"));

  char ip[16] = {0};
  char port[6] = {0};
  uint32_t size = 0;
  cr_assert_not(parse_stick_packet(&cpu, packet, ip, port, &size));

  log_destroy(cpu.logger);
}

/* ── receive_max_segment_size ──────────────────────────────────────────── */

Test(cpu_receive_max_segment_size, parses_the_announced_size)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);
  int size = 4096;
  cr_assert(send_buffer(OP_MAX_SEGMENT_SIZE, &size, sizeof(size), server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(receive_max_segment_size(&cpu));
  cr_assert_eq(cpu.max_segment_size, 4096);

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_receive_max_segment_size, fails_on_a_wrong_op_code)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);
  int bogus = 0;
  cr_assert(send_buffer(OP_ID_CPU, &bogus, sizeof(bogus), server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(receive_max_segment_size(&cpu));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

/* ── listen_kernel_memory ──────────────────────────────────────────────── */

Test(cpu_listen_kernel_memory, stops_and_reports_failure_on_a_dead_socket)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);
  socket_destroy(server_fd); /* the next read now sees OP_CODE_ERROR */

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(listen_kernel_memory(&cpu));

  socket_destroy(client_fd);
  log_destroy(cpu.logger);
}

Test(cpu_listen_kernel_memory,
     stops_and_reports_failure_on_an_unrecognized_op_code)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(listen_kernel_memory(&cpu));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_listen_kernel_memory,
     stops_and_reports_success_once_an_instruction_arrives)
{
  t_socket* server_fd;
  t_socket* client_fd = cpu_connected_pair(&server_fd);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "SET AX 5", server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(listen_kernel_memory(&cpu));

  /* listen_kernel_memory only peeks the op code for OP_SEND_INSTRUCTION and
   * leaves the payload for the caller to read. */
  free(receive_string(client_fd));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  log_destroy(cpu.logger);
}

/* ── listen_kernel_memory: stick announcements ─────────────────────────── */

static void send_stick_announcement(t_socket* km_peer, const char* port,
                                    bool complete)
{
  t_packet* packet = create_packet(OP_PACKET);
  packet_append_string(packet, "127.0.0.1");
  packet_append_string(packet, (char*)port);
  if (complete)
  {
    int size = 128;
    packet_append(packet, &size, sizeof(int));
  }
  cr_assert(send_packet(packet, km_peer));
  destroy_packet(packet);
}

struct stick_stub
{
  t_socket* listener;
  t_socket* accepted;
};

/* Plays a Memory Stick accepting the cpu: handshake, then its CPU id. */
static int stick_stub_thread(void* arg)
{
  struct stick_stub* stub = arg;
  stub->accepted = socket_accept(stub->listener, false);
  receive_handshake(stub->accepted);
  send_handshake(MID_MEMORY_STICK, stub->accepted);
  receive_op_code(stub->accepted);
  free(receive_string(stub->accepted));
  return 0;
}

Test(cpu_listen_kernel_memory, connects_an_announced_stick_and_keeps_listening)
{
  t_socket* km_peer;
  t_socket* km_client = cpu_connected_pair(&km_peer);
  t_cpu cpu = {.socket_kernel_memory = km_client, .id = "2"};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();

  char port[16];
  struct stick_stub stub = {.listener = cpu_listen_ephemeral(port, 16)};
  thrd_t thread;
  thrd_create(&thread, stick_stub_thread, &stub);

  send_stick_announcement(km_peer, port, true);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", km_peer));

  cr_assert(listen_kernel_memory(&cpu));
  cr_assert_eq(list_size(cpu.memory_sticks), 1);
  free(receive_string(km_client)); /* the instruction, left for the caller */

  thrd_join(thread, nullptr);
  t_memory_stick_info* stick = list_get(cpu.memory_sticks, 0);
  socket_destroy(stick->socket_ms);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  socket_destroy(stub.accepted);
  socket_destroy(stub.listener);
  socket_destroy(km_client);
  socket_destroy(km_peer);
  log_destroy(cpu.logger);
}

Test(cpu_listen_kernel_memory, fails_when_an_announced_stick_cannot_be_used)
{
  t_socket* km_peer;
  t_socket* km_client = cpu_connected_pair(&km_peer);
  t_cpu cpu = {.socket_kernel_memory = km_client, .id = "2"};
  cpu.logger = cpu_quiet_logger();
  cpu.memory_sticks = list_create();

  send_stick_announcement(km_peer, "1", false); /* missing its size */
  cr_assert_not(listen_kernel_memory(&cpu));
  cr_assert_eq(list_size(cpu.memory_sticks), 0);

  list_destroy(cpu.memory_sticks);
  socket_destroy(km_client);
  socket_destroy(km_peer);
  log_destroy(cpu.logger);
}

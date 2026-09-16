#include "cpu/kernel_memory_protocol.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

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
  int server_fd;
  int client_fd = cpu_connected_pair(&server_fd);
  int size = 4096;
  cr_assert(send_buffer(OP_MAX_SEGMENT_SIZE, &size, sizeof(size), server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(receive_max_segment_size(&cpu));
  cr_assert_eq(cpu.max_segment_size, 4096);

  close(client_fd);
  close(server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_receive_max_segment_size, fails_on_a_wrong_op_code)
{
  int server_fd;
  int client_fd = cpu_connected_pair(&server_fd);
  int bogus = 0;
  cr_assert(send_buffer(OP_ID_CPU, &bogus, sizeof(bogus), server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(receive_max_segment_size(&cpu));

  close(client_fd);
  close(server_fd);
  log_destroy(cpu.logger);
}

/* ── listen_kernel_memory ──────────────────────────────────────────────── */

Test(cpu_listen_kernel_memory, stops_and_reports_failure_on_a_dead_socket)
{
  int server_fd;
  int client_fd = cpu_connected_pair(&server_fd);
  close(server_fd); /* the next read now sees OP_CODE_ERROR */

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(listen_kernel_memory(&cpu));

  close(client_fd);
  log_destroy(cpu.logger);
}

Test(cpu_listen_kernel_memory,
     stops_and_reports_failure_on_an_unrecognized_op_code)
{
  int server_fd;
  int client_fd = cpu_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "not expected here", server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_not(listen_kernel_memory(&cpu));

  close(client_fd);
  close(server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_listen_kernel_memory,
     stops_and_reports_success_once_an_instruction_arrives)
{
  int server_fd;
  int client_fd = cpu_connected_pair(&server_fd);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "SET AX 5", server_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(listen_kernel_memory(&cpu));

  /* listen_kernel_memory only peeks the op code for OP_SEND_INSTRUCTION and
   * leaves the payload for the caller to read. */
  free(receive_string(client_fd));

  close(client_fd);
  close(server_fd);
  log_destroy(cpu.logger);
}

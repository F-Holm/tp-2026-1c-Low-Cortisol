#include "cpu/cpu.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cleanup.h"
#include "support.h"
#include "utils/collections/list.h"

/* ── decode_stage ──────────────────────────────────────────────────────── */

Test(cpu_decode, a_bare_instruction_has_no_parameters)
{
  t_instruction* instruction = decode_stage("NOOP");
  cr_assert_str_eq(instruction->name, "NOOP");
  cr_assert_eq(instruction->parameter_count, 0);
  destroy_instruction(instruction);
}

Test(cpu_decode, parameters_are_split_on_spaces)
{
  t_instruction* instruction = decode_stage("SET AX 5");
  cr_assert_str_eq(instruction->name, "SET");
  cr_assert_eq(instruction->parameter_count, 2);
  cr_assert_str_eq(instruction->parameters[0], "AX");
  cr_assert_str_eq(instruction->parameters[1], "5");
  destroy_instruction(instruction);
}

Test(cpu_decode, keeps_up_to_three_parameters)
{
  t_instruction* instruction = decode_stage("COPY_MEM a b c");
  cr_assert_eq(instruction->parameter_count, 3);
  cr_assert_str_eq(instruction->parameters[2], "c");
  destroy_instruction(instruction);
}

/* ── parse_stick_packet ────────────────────────────────────────────────── */

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

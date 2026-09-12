#include "cpu/cpu.h"

#include <criterion/criterion.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "cpu/cleanup.h"
#include "cpu/handlers.h"
#include "cpu/initializer.h"
#include "support.h"
#include "utils/collections/dictionary.h"
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

/* ── receive_pid ───────────────────────────────────────────────────────── */

Test(cpu_cpu, receive_pid_returns_the_pid_on_resume_process)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  uint32_t pid = 42;
  cr_assert(send_buffer(OP_RESUME_PROCESS, &pid, sizeof(pid), peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), 42);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_pid_returns_max_when_the_scheduler_disconnects)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  close(peer_fd);

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), UINT32_MAX);

  close(client_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_pid_returns_max_on_an_unexpected_op_code)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_ID_CPU, "unexpected", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), UINT32_MAX);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── request_context ──────────────────────────────────────────────────── */

Test(cpu_cpu, request_context_sends_the_pid_to_kernel_memory)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(request_context(&cpu, 7));

  cr_assert_eq(receive_op_code(peer_fd), OP_REQUEST_CONTEXT);
  int size;
  void* buffer = receive_buffer(&size, peer_fd);
  cr_assert_eq(*(uint32_t*)buffer, 7);
  free(buffer);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_context ───────────────────────────────────────────────────── */

Test(cpu_cpu, receive_context_copies_the_registers_off_the_wire)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_context* sent = cpu_make_context();
  sent->registers->PC = 5;
  sent->registers->EAX = 99;
  cr_assert(send_buffer(OP_SEND_CONTEXT, sent->registers, sizeof(t_registers),
                        peer_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  /* In production the op code is consumed by listen_kernel_memory() first;
   * simulate that here before calling receive_context() directly. */
  cr_assert_eq(receive_op_code(client_fd), OP_SEND_CONTEXT);
  t_registers* received = receive_context(&cpu);
  cr_assert_eq(received->PC, 5);
  cr_assert_eq(received->EAX, 99);

  free(received);
  cpu_destroy_context(sent);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_segment_table ────────────────────────────────────────────── */

Test(cpu_cpu, receive_segment_table_reads_the_segments_off_the_wire)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_packet* packet = create_packet(OP_SEGMENT_TABLE);
  t_segment* seg0 = cpu_make_segment(0, 100, 64);
  t_segment* seg1 = cpu_make_segment(1, 200, 64);
  packet_append(packet, seg0, sizeof(t_segment));
  packet_append(packet, seg1, sizeof(t_segment));
  cr_assert(send_packet(packet, peer_fd));
  destroy_packet(packet);
  free(seg0);
  free(seg1);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  t_context* context = cpu_make_context(); /* starts with an empty table */

  cr_assert_eq(receive_op_code(client_fd), OP_SEGMENT_TABLE);
  context->segment_table = receive_segment_table(&cpu, context);
  cr_assert_eq(list_size(context->segment_table), 2);
  cr_assert_eq(((t_segment*)list_get(context->segment_table, 0))->base, 100);
  cr_assert_eq(((t_segment*)list_get(context->segment_table, 1))->base, 200);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_segment_table_frees_the_previous_table_first)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_packet* packet = create_packet(OP_SEGMENT_TABLE); /* an empty table */
  cr_assert(send_packet(packet, peer_fd));
  destroy_packet(packet);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));

  cr_assert_eq(receive_op_code(client_fd), OP_SEGMENT_TABLE);
  context->segment_table = receive_segment_table(&cpu, context);
  cr_assert_eq(list_size(context->segment_table), 0);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── request_instruction ───────────────────────────────────────────────── */

Test(cpu_cpu, request_instruction_sends_the_pid_and_program_counter)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(request_instruction(&cpu, 3, 12));

  cr_assert_eq(receive_op_code(peer_fd), OP_NEXT_INSTRUCTION);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(list_size(fields), 2);
  cr_assert_eq(*(uint32_t*)list_get(fields, 0), 3);
  cr_assert_eq(*(uint32_t*)list_get(fields, 1), 12);

  list_destroy_and_destroy_elements(fields, free);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_instruction ───────────────────────────────────────────────── */

Test(cpu_cpu, receive_instruction_reads_the_raw_instruction_string)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "SET EAX 5", peer_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_op_code(client_fd), OP_SEND_INSTRUCTION);
  char* instruction = receive_instruction(&cpu);
  cr_assert_str_eq(instruction, "SET EAX 5");

  free(instruction);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── fetch_stage ───────────────────────────────────────────────────────── */

Test(cpu_cpu, fetch_stage_requests_and_returns_the_instruction)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", peer_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  char* instruction = fetch_stage(&cpu, 1, 0);
  cr_assert_str_eq(instruction, "NOOP");

  cr_assert_eq(receive_op_code(peer_fd), OP_NEXT_INSTRUCTION);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(*(uint32_t*)list_get(fields, 0), 1);
  cr_assert_eq(*(uint32_t*)list_get(fields, 1), 0);
  list_destroy_and_destroy_elements(fields, free);

  free(instruction);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, fetch_stage_fails_when_kernel_memory_disconnects)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  close(peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_null(fetch_stage(&cpu, 1, 0));

  close(client_fd);
  log_destroy(cpu.logger);
}

/* ── execute_stage ─────────────────────────────────────────────────────── */

Test(cpu_cpu, execute_stage_reports_an_error_for_an_unknown_instruction)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.handlers = dictionary_create();

  t_context* context = cpu_make_context();
  t_instruction instruction = {.name = "UNKNOWN", .parameter_count = 0};

  cr_assert_eq(execute_stage(&cpu, context, &instruction, 1), EB_ERROR);

  dictionary_destroy(cpu.handlers);
  cpu_destroy_context(context);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, execute_stage_calls_through_to_the_registered_handler)
{
  t_cpu cpu = {0};
  cpu.logger = cpu_quiet_logger();
  cpu.handlers = dictionary_create();
  dictionary_put(cpu.handlers, "NOOP", (void*)handler_noop);

  t_context* context = cpu_make_context();
  t_instruction instruction = {.name = "NOOP", .parameter_count = 0};

  cr_assert_eq(execute_stage(&cpu, context, &instruction, 1), EB_TRUE);

  dictionary_destroy(cpu.handlers);
  cpu_destroy_context(context);
  log_destroy(cpu.logger);
}

/* ── check_interrupt ───────────────────────────────────────────────────── */

Test(cpu_cpu, check_interrupt_reports_no_table_when_memory_ran_out)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_INTERRUPT,
                        "there is not enough memory for this instruction",
                        peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_NO_TABLE);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_false_for_any_other_interrupt_reason)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_INTERRUPT, "quantum expired", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_FALSE);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_true_when_there_is_no_interrupt)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_NO_INTERRUPT, "keep going", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_TRUE);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_error_on_an_explicit_code_error)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  /* OP_CODE_ERROR is 0; send it explicitly (rather than really closing the
   * socket) so the follow-up receive_string() in production code would still
   * have well-defined data to read if it were ever called on this path. */
  cr_assert(send_string(OP_CODE_ERROR, "disconnected", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_ERROR);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_error_on_an_unrecognized_op_code)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_ID_CPU, "garbage", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_ERROR);

  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── send_updated_context ──────────────────────────────────────────────── */

Test(cpu_cpu, send_updated_context_sends_the_pid_and_registers)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  t_registers registers = {0};
  registers.PC = 7;
  registers.EAX = 55;

  cr_assert(send_updated_context(&cpu, 3, &registers));

  cr_assert_eq(receive_op_code(peer_fd), OP_UPDATED_CONTEXT);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(*(uint32_t*)list_get(fields, 0), 3);
  t_registers* received = list_get(fields, 1);
  cr_assert_eq(received->PC, 7);
  cr_assert_eq(received->EAX, 55);

  list_destroy_and_destroy_elements(fields, free);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── update_segment_table ──────────────────────────────────────────────── */

Test(cpu_cpu, update_segment_table_requests_and_stores_the_refreshed_table)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);

  /* update_segment_table sends its request first and only then listens for
   * the reply, so the reply must already be queued before we call it. */
  t_packet* reply = create_packet(OP_SEGMENT_TABLE); /* an empty table */
  cr_assert(send_packet(reply, peer_fd));
  destroy_packet(reply);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));

  cr_assert(update_segment_table(&cpu, 9, context));
  cr_assert_eq(list_size(context->segment_table), 0);

  cr_assert_eq(receive_op_code(peer_fd), OP_UPDATED_SEGMENT_TABLE);
  int size;
  void* buffer = receive_buffer(&size, peer_fd);
  cr_assert_eq(*(uint32_t*)buffer, 9);
  free(buffer);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── run_instruction_cycle ─────────────────────────────────────────────── */

Test(cpu_cpu, run_instruction_cycle_runs_one_noop_and_reports_back)
{
  int km_peer, sched_peer;
  int km_client = cpu_connected_pair(&km_peer);
  int sched_client = cpu_connected_pair(&sched_peer);

  /* This test drives the whole function from a single thread: every reply
   * the two peers give must already be sitting on the wire before the call
   * below, in the exact order the code under test will read it. Requests the
   * CPU sends are only read back afterwards, since nothing on this side
   * blocks waiting for them to be consumed. */
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", km_peer));
  /* Any interrupt reason other than the "no more memory" one ends the cycle
   * after a single instruction (see check_interrupt: OP_NO_INTERRUPT would
   * instead make the cycle keep going and fetch another instruction). */
  cr_assert(send_string(OP_INTERRUPT, "quantum expired", sched_peer));

  t_cpu cpu = {.socket_kernel_memory = km_client,
               .socket_kernel_scheduler = sched_client};
  cpu.logger = cpu_quiet_logger();
  cpu.handlers = dictionary_create();
  register_handlers(cpu.handlers);

  t_context* context = cpu_make_context();

  cr_assert(run_instruction_cycle(&cpu, 4, context));
  cr_assert_eq(context->registers->PC, 1); /* NOOP just advances the PC */

  /* what the CPU sent to Kernel Memory: the instruction request, then the
   * updated context once the cycle ended. */
  cr_assert_eq(receive_op_code(km_peer), OP_NEXT_INSTRUCTION);
  t_list* instruction_fields = receive_packet(km_peer);
  cr_assert_eq(*(uint32_t*)list_get(instruction_fields, 0), 4);
  cr_assert_eq(*(uint32_t*)list_get(instruction_fields, 1), 0);
  list_destroy_and_destroy_elements(instruction_fields, free);

  cr_assert_eq(receive_op_code(km_peer), OP_UPDATED_CONTEXT);
  t_list* context_fields = receive_packet(km_peer);
  cr_assert_eq(*(uint32_t*)list_get(context_fields, 0), 4);
  cr_assert_eq(((t_registers*)list_get(context_fields, 1))->PC, 1);
  list_destroy_and_destroy_elements(context_fields, free);

  /* what the CPU sent to the Kernel Scheduler: the end-of-cycle notice. */
  cr_assert_eq(receive_op_code(sched_peer), OP_CPU_CYCLE_OK);
  free(receive_string(sched_peer));

  dictionary_destroy(cpu.handlers);
  cpu_destroy_context(context);
  close(km_client);
  close(km_peer);
  close(sched_client);
  close(sched_peer);
  log_destroy(cpu.logger);
}

/* ── run_instruction_loop ──────────────────────────────────────────────── */

Test(cpu_cpu, run_instruction_loop_runs_one_cycle_then_stops_on_disconnect)
{
  int km_peer, sched_peer;
  int km_client = cpu_connected_pair(&km_peer);
  int sched_client = cpu_connected_pair(&sched_peer);

  uint32_t pid = 6;
  t_registers registers = {0};

  /* Kernel Scheduler script: hand out a PID, let the cycle's interrupt check
   * end it, then hang up so the loop's next receive_pid() call ends the
   * loop. Already-sent data survives a local close(), so queueing all of
   * this up front and closing right away is safe and deterministic. */
  cr_assert(send_buffer(OP_RESUME_PROCESS, &pid, sizeof(pid), sched_peer));
  cr_assert(send_string(OP_INTERRUPT, "quantum expired", sched_peer));
  close(sched_peer);

  /* Kernel Memory script: hand back a context, an empty segment table, and
   * one instruction. */
  cr_assert(
      send_buffer(OP_SEND_CONTEXT, &registers, sizeof(t_registers), km_peer));
  t_packet* segment_table_reply = create_packet(OP_SEGMENT_TABLE);
  cr_assert(send_packet(segment_table_reply, km_peer));
  destroy_packet(segment_table_reply);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", km_peer));

  t_cpu cpu = {.socket_kernel_memory = km_client,
               .socket_kernel_scheduler = sched_client};
  cpu.logger = cpu_quiet_logger();
  cpu.handlers = dictionary_create();
  register_handlers(cpu.handlers);

  run_instruction_loop(&cpu); /* returns once the scheduler script runs out */

  cr_assert_eq(receive_op_code(km_peer), OP_REQUEST_CONTEXT);
  int size;
  void* pid_buffer = receive_buffer(&size, km_peer);
  cr_assert_eq(*(uint32_t*)pid_buffer, 6);
  free(pid_buffer);

  cr_assert_eq(receive_op_code(km_peer), OP_NEXT_INSTRUCTION);
  t_list* instruction_fields = receive_packet(km_peer);
  list_destroy_and_destroy_elements(instruction_fields, free);

  cr_assert_eq(receive_op_code(km_peer), OP_UPDATED_CONTEXT);
  t_list* context_fields = receive_packet(km_peer);
  list_destroy_and_destroy_elements(context_fields, free);

  dictionary_destroy(cpu.handlers);
  close(km_client);
  close(km_peer);
  close(sched_client);
  log_destroy(cpu.logger);
}

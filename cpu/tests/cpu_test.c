#include "cpu/cpu.h"

#include <criterion/criterion.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cleanup.h"
#include "cpu/handlers.h"
#include "cpu/initializer.h"
#include "cpu/registers.h"
#include "support.h"
#include "utils/collections/dictionary.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/sockets.h"

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
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  uint32_t pid = 42;
  cr_assert(send_buffer(OP_RESUME_PROCESS, &pid, sizeof(pid), peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), 42);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_pid_returns_max_when_the_scheduler_disconnects)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  socket_destroy(peer_fd);

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), UINT32_MAX);

  socket_destroy(client_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_pid_returns_max_on_an_unexpected_op_code)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_ID_CPU, "unexpected", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_pid(&cpu), UINT32_MAX);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── request_context ──────────────────────────────────────────────────── */

Test(cpu_cpu, request_context_sends_the_pid_to_kernel_memory)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(request_context(&cpu, 7));

  cr_assert_eq(receive_op_code(peer_fd), OP_REQUEST_CONTEXT);
  int size;
  void* buffer = receive_buffer(&size, peer_fd);
  cr_assert_eq(*(uint32_t*)buffer, 7);
  free(buffer);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_context ───────────────────────────────────────────────────── */

Test(cpu_cpu, receive_context_copies_the_registers_off_the_wire)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_segment_table ────────────────────────────────────────────── */

Test(cpu_cpu, receive_segment_table_reads_the_segments_off_the_wire)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, receive_segment_table_frees_the_previous_table_first)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── request_instruction ───────────────────────────────────────────────── */

Test(cpu_cpu, request_instruction_sends_the_pid_and_program_counter)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert(request_instruction(&cpu, 3, 12));

  cr_assert_eq(receive_op_code(peer_fd), OP_NEXT_INSTRUCTION);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(list_size(fields), 2);
  cr_assert_eq(*(uint32_t*)list_get(fields, 0), 3);
  cr_assert_eq(*(uint32_t*)list_get(fields, 1), 12);

  list_destroy_and_destroy_elements(fields, free);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── receive_instruction ───────────────────────────────────────────────── */

Test(cpu_cpu, receive_instruction_reads_the_raw_instruction_string)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "SET EAX 5", peer_fd));

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(receive_op_code(client_fd), OP_SEND_INSTRUCTION);
  char* instruction = receive_instruction(&cpu);
  cr_assert_str_eq(instruction, "SET EAX 5");

  free(instruction);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── fetch_stage ───────────────────────────────────────────────────────── */

Test(cpu_cpu, fetch_stage_requests_and_returns_the_instruction)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, fetch_stage_fails_when_kernel_memory_disconnects)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  socket_destroy(peer_fd);

  t_cpu cpu = {.socket_kernel_memory = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_null(fetch_stage(&cpu, 1, 0));

  socket_destroy(client_fd);
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
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_INTERRUPT,
                        "there is not enough memory for this instruction",
                        peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_NO_TABLE);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_false_for_any_other_interrupt_reason)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_INTERRUPT, "quantum expired", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_FALSE);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_true_when_there_is_no_interrupt)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_NO_INTERRUPT, "keep going", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_TRUE);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_error_on_an_explicit_code_error)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  /* OP_CODE_ERROR is 0; send it explicitly (rather than really closing the
   * socket) so the follow-up receive_string() in production code would still
   * have well-defined data to read if it were ever called on this path. */
  cr_assert(send_string(OP_CODE_ERROR, "disconnected", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_ERROR);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_cpu, check_interrupt_reports_error_on_an_unrecognized_op_code)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  cr_assert(send_string(OP_ID_CPU, "garbage", peer_fd));

  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();

  cr_assert_eq(check_interrupt(&cpu, 1), EB_ERROR);

  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── send_updated_context ──────────────────────────────────────────────── */

Test(cpu_cpu, send_updated_context_sends_the_pid_and_registers)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── update_segment_table ──────────────────────────────────────────────── */

Test(cpu_cpu, update_segment_table_requests_and_stores_the_refreshed_table)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);

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
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── run_instruction_cycle ─────────────────────────────────────────────── */

Test(cpu_cpu, run_instruction_cycle_runs_one_noop_and_reports_back)
{
  t_socket *km_peer, *sched_peer;
  t_socket* km_client = cpu_connected_pair(&km_peer);
  t_socket* sched_client = cpu_connected_pair(&sched_peer);

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
  socket_destroy(km_client);
  socket_destroy(km_peer);
  socket_destroy(sched_client);
  socket_destroy(sched_peer);
  log_destroy(cpu.logger);
}

/* ── run_instruction_loop ──────────────────────────────────────────────── */

Test(cpu_cpu, run_instruction_loop_runs_one_cycle_then_stops_on_disconnect)
{
  t_socket *km_peer, *sched_peer;
  t_socket* km_client = cpu_connected_pair(&km_peer);
  t_socket* sched_client = cpu_connected_pair(&sched_peer);

  uint32_t pid = 6;
  t_registers registers = {0};

  /* Kernel Scheduler script: hand out a PID, let the cycle's interrupt check
   * end it, then hang up so the loop's next receive_pid() call ends the
   * loop. Already-sent data survives a local close(), so queueing all of
   * this up front and closing right away is safe and deterministic. */
  cr_assert(send_buffer(OP_RESUME_PROCESS, &pid, sizeof(pid), sched_peer));
  cr_assert(send_string(OP_INTERRUPT, "quantum expired", sched_peer));
  socket_destroy(sched_peer);

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
  socket_destroy(km_client);
  socket_destroy(km_peer);
  socket_destroy(sched_client);
  log_destroy(cpu.logger);
}

/* ── run_instruction_cycle / run_instruction_loop failure paths ────────── */

/* A cpu wired to a Kernel Memory peer and a Kernel Scheduler peer, with every
 * handler registered. As in the tests above, every reply a peer gives is
 * queued before the call under test, in the order the cpu reads them. */
typedef struct
{
  t_cpu cpu;
  t_socket* km_peer;
  t_socket* sched_peer;
  t_context* context;
} t_cycle_fixture;

static void cycle_fixture_start(t_cycle_fixture* fx)
{
  memset(fx, 0, sizeof(*fx));
  fx->cpu.socket_kernel_memory = cpu_connected_pair(&fx->km_peer);
  fx->cpu.socket_kernel_scheduler = cpu_connected_pair(&fx->sched_peer);
  fx->cpu.logger = cpu_quiet_logger();
  fx->cpu.handlers = dictionary_create();
  register_handlers(fx->cpu.handlers);
  fx->context = cpu_make_context();
}

static void cycle_fixture_end(t_cycle_fixture* fx)
{
  dictionary_destroy(fx->cpu.handlers);
  cpu_destroy_context(fx->context);
  socket_destroy(fx->cpu.socket_kernel_memory);
  socket_destroy(fx->km_peer);
  socket_destroy(fx->cpu.socket_kernel_scheduler);
  socket_destroy(fx->sched_peer);
  log_destroy(fx->cpu.logger);
}

/* Reads the next op code @p peer got and drops whatever packet followed. */
static int drain_packet(t_socket* peer)
{
  int op_code = receive_op_code(peer);
  list_destroy_and_destroy_elements(receive_packet(peer), free);
  return op_code;
}

static void queue_segment_table(t_socket* km_peer, int segment_count)
{
  t_packet* reply = create_packet(OP_SEGMENT_TABLE);
  for (int i = 0; i < segment_count; i++)
  {
    t_segment segment = {.id = i, .base = i * 32, .size = 32};
    packet_append(reply, &segment, sizeof(segment));
  }
  cr_assert(send_packet(reply, km_peer));
  destroy_packet(reply);
}

Test(cpu_cpu,
     run_instruction_cycle_fails_when_the_instruction_cannot_be_fetched)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);

  socket_shutdown(fx.cpu.socket_kernel_memory, SOCKET_SHUTDOWN_WRITE);
  cr_assert_not(request_instruction(&fx.cpu, 1, 0));
  cr_assert_null(fetch_stage(&fx.cpu, 1, 0));
  cr_assert_not(run_instruction_cycle(&fx.cpu, 1, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_fails_on_an_unknown_instruction)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "BOGUS 1 2", fx.km_peer));

  cr_assert_not(run_instruction_cycle(&fx.cpu, 1, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_fails_when_the_cycle_end_cannot_be_sent)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", fx.km_peer));

  socket_shutdown(fx.cpu.socket_kernel_scheduler, SOCKET_SHUTDOWN_WRITE);
  cr_assert_not(run_instruction_cycle(&fx.cpu, 1, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_fails_when_the_interrupt_check_fails)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "NOOP", fx.km_peer));
  cr_assert(send_string(OP_ID_CPU, "unexpected", fx.sched_peer));

  cr_assert_not(run_instruction_cycle(&fx.cpu, 1, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_refreshes_the_segment_table_after_mem_alloc)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "MEM_ALLOC 1 32", fx.km_peer));
  queue_segment_table(fx.km_peer, 2); /* the refreshed table */
  cr_assert(send_string(OP_INTERRUPT, "syscall", fx.sched_peer));

  cr_assert(run_instruction_cycle(&fx.cpu, 3, fx.context));
  cr_assert_eq(list_size(fx.context->segment_table), 2);
  cr_assert_not(fx.context->segment_changed);

  cr_assert_eq(drain_packet(fx.km_peer), OP_NEXT_INSTRUCTION);
  cr_assert_eq(receive_op_code(fx.km_peer), OP_UPDATED_SEGMENT_TABLE);
  int size;
  void* pid = receive_buffer(&size, fx.km_peer);
  cr_assert_eq(*(uint32_t*)pid, 3);
  free(pid);
  cr_assert_eq(drain_packet(fx.km_peer), OP_UPDATED_CONTEXT);

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_skips_the_refresh_when_memory_ran_out)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "MEM_ALLOC 1 32", fx.km_peer));
  cr_assert(send_string(OP_INTERRUPT,
                        "there is not enough memory for this instruction",
                        fx.sched_peer));

  cr_assert(run_instruction_cycle(&fx.cpu, 3, fx.context));
  cr_assert_not(fx.context->segment_changed);

  /* no OP_UPDATED_SEGMENT_TABLE in between */
  cr_assert_eq(drain_packet(fx.km_peer), OP_NEXT_INSTRUCTION);
  cr_assert_eq(drain_packet(fx.km_peer), OP_UPDATED_CONTEXT);

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_cycle_fails_when_the_segment_table_refresh_fails)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "MEM_ALLOC 1 32", fx.km_peer));
  cr_assert(send_string(OP_ID_CPU, "not a segment table", fx.km_peer));
  cr_assert(send_string(OP_INTERRUPT, "syscall", fx.sched_peer));

  cr_assert_not(run_instruction_cycle(&fx.cpu, 3, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, update_segment_table_fails_when_the_request_cannot_be_sent)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);

  socket_shutdown(fx.cpu.socket_kernel_memory, SOCKET_SHUTDOWN_WRITE);
  cr_assert_not(update_segment_table(&fx.cpu, 1, fx.context));

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, send_updated_context_fails_when_kernel_memory_is_gone)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);

  socket_shutdown(fx.cpu.socket_kernel_memory, SOCKET_SHUTDOWN_WRITE);
  cr_assert_not(send_updated_context(&fx.cpu, 1, fx.context->registers));

  cycle_fixture_end(&fx);
}

/* run_instruction_loop builds its own context, so these only queue the
 * Kernel Scheduler's PID and the Kernel Memory replies, then hang up. */
static void queue_resume(t_cycle_fixture* fx, uint32_t pid)
{
  cr_assert(send_buffer(OP_RESUME_PROCESS, &pid, sizeof(pid), fx->sched_peer));
}

Test(cpu_cpu, run_instruction_loop_stops_when_the_context_cannot_be_requested)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  queue_resume(&fx, 1);

  socket_shutdown(fx.cpu.socket_kernel_memory, SOCKET_SHUTDOWN_WRITE);
  run_instruction_loop(&fx.cpu); /* must return instead of looping */

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_loop_stops_when_kernel_memory_sends_no_context)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  queue_resume(&fx, 1);
  cr_assert(send_string(OP_ID_CPU, "not a context", fx.km_peer));

  run_instruction_loop(&fx.cpu);

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_loop_stops_when_kernel_memory_sends_no_table)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  queue_resume(&fx, 1);
  t_registers registers = {0};
  cr_assert(
      send_buffer(OP_SEND_CONTEXT, &registers, sizeof(registers), fx.km_peer));
  cr_assert(send_string(OP_ID_CPU, "not a segment table", fx.km_peer));

  run_instruction_loop(&fx.cpu);

  cycle_fixture_end(&fx);
}

Test(cpu_cpu, run_instruction_loop_stops_when_the_cycle_fails)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  queue_resume(&fx, 1);
  t_registers registers = {0};
  cr_assert(
      send_buffer(OP_SEND_CONTEXT, &registers, sizeof(registers), fx.km_peer));
  queue_segment_table(fx.km_peer, 1);
  cr_assert(send_string(OP_SEND_INSTRUCTION, "BOGUS", fx.km_peer));

  run_instruction_loop(&fx.cpu);

  cycle_fixture_end(&fx);
}

/* Whether @p socket has bytes waiting to be read, without blocking. */
static bool has_pending_data(t_socket* socket)
{
  struct pollfd pfd = {.fd = socket->handle, .events = POLLIN};
  return poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN);
}

Test(cpu_cpu, a_segmentation_fault_is_not_followed_by_a_cycle_confirmation)
{
  t_cycle_fixture fx;
  cycle_fixture_start(&fx);
  fx.cpu.max_segment_size = 100;
  list_add(fx.context->segment_table, cpu_make_segment(0, 0, 32));
  fx.context->registers->SI = 4;  /* inside segment 0 */
  fx.context->registers->DI = 90; /* past segment 0's end */
  set_register(fx.context->registers, "EAX", 4);

  cr_assert(send_string(OP_SEND_INSTRUCTION, "COPY_MEM EAX", fx.km_peer));
  cr_assert(send_string(OP_INTERRUPT, "segmentation fault", fx.sched_peer));

  cr_assert(run_instruction_cycle(&fx.cpu, 1, fx.context));

  /* the scheduler hears about the fault, and nothing else: an extra
   * OP_CPU_CYCLE_OK would be read as the next process's first message */
  cr_assert_eq(receive_op_code(fx.sched_peer), OP_SEG_FAULT);
  free(receive_string(fx.sched_peer));
  cr_assert_not(has_pending_data(fx.sched_peer));

  cycle_fixture_end(&fx);
}

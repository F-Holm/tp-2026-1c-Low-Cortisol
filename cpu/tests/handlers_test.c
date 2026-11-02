#include "cpu/handlers.h"

#include <criterion/criterion.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/syscalls.h"

/* These handlers only touch the context's registers, so the cpu and the pid are
 * placeholders. */

static t_instruction make_instruction(char* name, char* p0, char* p1)
{
  t_instruction instruction = {.name = name, .parameter_count = 0};
  if (p0 != nullptr)
  {
    instruction.parameters[0] = p0;
    instruction.parameter_count++;
  }
  if (p1 != nullptr)
  {
    instruction.parameters[1] = p1;
    instruction.parameter_count++;
  }
  return instruction;
}

Test(cpu_handlers, noop_does_nothing_and_succeeds)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  context->registers->EAX = 5;

  t_instruction instruction = make_instruction("NOOP", nullptr, nullptr);
  cr_assert_eq(handler_noop(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(context->registers->EAX, 5);

  cpu_destroy_context(context);
}

Test(cpu_handlers, set_writes_a_literal_into_a_register)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("SET", "EAX", "42");
  cr_assert_eq(handler_set(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);

  cpu_destroy_context(context);
}

Test(cpu_handlers, sum_adds_the_second_register_into_the_first)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 10);
  set_register(context->registers, "EBX", 32);

  t_instruction instruction = make_instruction("SUM", "EAX", "EBX");
  cr_assert_eq(handler_sum(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);
  cr_assert_eq(get_register(context->registers, "EBX"), 32);

  cpu_destroy_context(context);
}

Test(cpu_handlers, sub_subtracts_the_second_register_from_the_first)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 50);
  set_register(context->registers, "EBX", 8);

  t_instruction instruction = make_instruction("SUB", "EAX", "EBX");
  cr_assert_eq(handler_sub(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 42);

  cpu_destroy_context(context);
}

Test(cpu_handlers, jnz_jumps_when_the_register_is_non_zero)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 1);
  set_register(context->registers, "PC", 5);

  t_instruction instruction = make_instruction("JNZ", "EAX", "99");
  cr_assert_eq(handler_jnz(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "PC"), 99);

  cpu_destroy_context(context);
}

Test(cpu_handlers, jnz_does_not_jump_when_the_register_is_zero)
{
  t_cpu cpu = {0};
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 0);
  set_register(context->registers, "PC", 5);

  t_instruction instruction = make_instruction("JNZ", "EAX", "99");
  cr_assert_eq(handler_jnz(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "PC"), 5);

  cpu_destroy_context(context);
}

/* ── syscalls forwarded to the Kernel Scheduler ───────────────────────────
 * None of these wait for a reply, so the fake peer only needs to receive
 * and check what was sent. */

Test(cpu_handlers, mem_alloc_sends_the_syscall_and_marks_segment_changed)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("MEM_ALLOC", "3", "1024");
  cr_assert_eq(handler_mem_alloc(&cpu, context, &instruction, 7), EB_FALSE);
  cr_assert(context->segment_changed);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MEM_ALLOC);
  int size;
  t_syscall_memory* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 7);
  cr_assert_eq(data->segment_id, 3);
  cr_assert_eq(data->size, 1024);
  free(data);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mem_free_sends_the_syscall_with_a_zero_size)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("MEM_FREE", "3", nullptr);
  cr_assert_eq(handler_mem_free(&cpu, context, &instruction, 7), EB_FALSE);
  cr_assert(context->segment_changed);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MEM_FREE);
  int size;
  t_syscall_memory* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 7);
  cr_assert_eq(data->segment_id, 3);
  cr_assert_eq(data->size, 0);
  free(data);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, sleep_sends_the_pid_and_the_blocked_time)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("SLEEP", "250", nullptr);
  cr_assert_eq(handler_sleep(&cpu, context, &instruction, 9), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_SLEEP);
  int size;
  t_sleep_request* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 9);
  cr_assert_eq(data->blocked_time_ms, 250);
  free(data);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, stdout_sends_the_address_and_byte_count_from_registers)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 100);
  set_register(context->registers, "EBX", 8);

  t_instruction instruction = make_instruction("STDOUT", "EAX", "EBX");
  cr_assert_eq(handler_stdout(&cpu, context, &instruction, 4), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_STDOUT);
  int size;
  t_stdout_request* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 4);
  cr_assert_eq(data->logical_address, 100);
  cr_assert_eq(data->bytes_to_write, 8);
  free(data);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, stdin_sends_the_address_and_byte_count_from_registers)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();
  set_register(context->registers, "EAX", 200);
  set_register(context->registers, "EBX", 16);

  t_instruction instruction = make_instruction("STDIN", "EAX", "EBX");
  cr_assert_eq(handler_stdin(&cpu, context, &instruction, 5), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_STDIN);
  int size;
  t_stdin_request* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 5);
  cr_assert_eq(data->logical_address, 200);
  cr_assert_eq(data->bytes_to_read, 16);
  free(data);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_create_sends_the_mutex_name)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction =
      make_instruction("MUTEX_CREATE", "my_mutex", nullptr);
  cr_assert_eq(handler_mutex_create(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_CREATE);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_lock_sends_the_mutex_name)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction =
      make_instruction("MUTEX_LOCK", "my_mutex", nullptr);
  cr_assert_eq(handler_mutex_lock(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_LOCK);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_unlock_sends_the_mutex_name)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction =
      make_instruction("MUTEX_UNLOCK", "my_mutex", nullptr);
  cr_assert_eq(handler_mutex_unlock(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_UNLOCK);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, init_proc_sends_the_script_path_and_priority)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("INIT_PROC", "script.txt", "3");
  cr_assert_eq(handler_init_proc(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_INIT_PROC);
  t_list* fields = receive_packet(peer_fd);
  cr_assert_eq(list_size(fields), 2);
  cr_assert_str_eq((char*)list_get(fields, 0), "script.txt");
  cr_assert_eq(*(int*)list_get(fields, 1), 3);
  list_destroy_and_destroy_elements(fields, free);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, exit_sends_the_process_finished_message)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("EXIT", nullptr, nullptr);
  cr_assert_eq(handler_exit(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_EXIT);
  char* message = receive_string(peer_fd);
  cr_assert_str_eq(message, "PROCESS FINISHED");
  free(message);

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── instructions that touch memory ────────────────────────────────────── */

Test(cpu_handlers, mov_in_reads_a_value_from_memory_into_a_register)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;
  cpu.memory_sticks = list_create();
  t_socket* stick_server_fd;
  t_socket* stick_client_fd = cpu_connected_pair(&stick_server_fd);
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_client_fd;
  list_add(cpu.memory_sticks, stick);

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));
  context->registers->SI = 4; /* logical address inside segment 0 */

  uint32_t stored_value = 55;
  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, &stored_value,
                        sizeof(stored_value), stick_server_fd));

  t_instruction instruction = make_instruction("MOV_IN", "EAX", nullptr);
  cr_assert_eq(handler_mov_in(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 55);

  cpu_destroy_context(context);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  socket_destroy(stick_client_fd);
  socket_destroy(stick_server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mov_out_writes_a_register_value_to_memory)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;
  cpu.memory_sticks = list_create();
  t_socket* stick_server_fd;
  t_socket* stick_client_fd = cpu_connected_pair(&stick_server_fd);
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_client_fd;
  list_add(cpu.memory_sticks, stick);

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));
  context->registers->DI = 4;
  set_register(context->registers, "EAX", 77);

  /* write_memory() is a synchronous request/response, so the stick's "done"
   * reply has to already be queued before the call -- receive_write_response
   * always reads an op code AND a string, whatever that op code is. */
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", stick_server_fd));

  t_instruction instruction = make_instruction("MOV_OUT", "EAX", nullptr);
  cr_assert_eq(handler_mov_out(&cpu, context, &instruction, 1), EB_TRUE);

  cr_assert_eq(receive_op_code(stick_server_fd), OP_MEMORY_STICK_WRITE);
  int size;
  void* packet = receive_buffer(&size, stick_server_fd);
  free(packet);

  cpu_destroy_context(context);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  socket_destroy(stick_client_fd);
  socket_destroy(stick_server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, copy_mem_stops_the_process_when_the_destination_faults)
{
  t_socket* peer_fd;
  t_socket* client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));
  context->registers->SI = 4;  /* valid source address */
  context->registers->DI = 90; /* logical address past segment 0's range */
  set_register(context->registers, "EAX", 4);

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  /* dst_addr's mmu() call reports a segmentation fault (segment 0 doesn't
   * cover offset 90), so the handler returns without ever touching src or
   * dst memory. */
  cr_assert_eq(handler_copy_mem(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SEG_FAULT);
  free(receive_string(peer_fd));

  cpu_destroy_context(context);
  socket_destroy(client_fd);
  socket_destroy(peer_fd);
  log_destroy(cpu.logger);
}

/* ── memory-instruction failure paths and COPY_MEM ─────────────────────── */

/* A cpu with a Kernel Scheduler peer and one stick covering [0, 100), running
 * a process whose segment 0 maps to [0, 32) and segment 1 to [32, 64). Segment
 * 2 maps to [200, 232): valid for the MMU, but no stick backs it. */
typedef struct
{
  t_cpu cpu;
  t_socket* scheduler_peer;
  t_socket* km_peer;
  t_socket* stick_peer;
  t_context* context;
} t_handler_fixture;

static void handler_fixture_start(t_handler_fixture* fx)
{
  memset(fx, 0, sizeof(*fx));
  fx->cpu.logger = cpu_quiet_logger();
  fx->cpu.max_segment_size = 100;
  fx->cpu.socket_kernel_scheduler = cpu_connected_pair(&fx->scheduler_peer);
  fx->cpu.socket_kernel_memory = cpu_connected_pair(&fx->km_peer);
  fx->cpu.memory_sticks = list_create();
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = cpu_connected_pair(&fx->stick_peer);
  list_add(fx->cpu.memory_sticks, stick);

  fx->context = cpu_make_context();
  list_add(fx->context->segment_table, cpu_make_segment(0, 0, 32));
  list_add(fx->context->segment_table, cpu_make_segment(1, 32, 32));
  list_add(fx->context->segment_table, cpu_make_segment(2, 200, 32));
}

static void handler_fixture_end(t_handler_fixture* fx)
{
  cpu_destroy_context(fx->context);
  t_memory_stick_info* stick = list_get(fx->cpu.memory_sticks, 0);
  socket_destroy(stick->socket_ms);
  socket_destroy(fx->stick_peer);
  list_destroy_and_destroy_elements(fx->cpu.memory_sticks, free);
  socket_destroy(fx->cpu.socket_kernel_scheduler);
  socket_destroy(fx->scheduler_peer);
  socket_destroy(fx->cpu.socket_kernel_memory);
  socket_destroy(fx->km_peer);
  log_destroy(fx->cpu.logger);
}

static void assert_seg_fault_sent(t_handler_fixture* fx)
{
  cr_assert_eq(receive_op_code(fx->scheduler_peer), OP_SEG_FAULT);
  free(receive_string(fx->scheduler_peer));
}

/* Logical addresses (max_segment_size = 100): segment * 100 + offset. */
#define SEG0_OFFSET(o) (0 + (o))
#define SEG1_OFFSET(o) (100 + (o))
#define SEG2_OFFSET(o) (200 + (o))
#define MISSING_SEGMENT 300
#define PAST_SEG0_END SEG0_OFFSET(30) /* 30 + 4 bytes > 32 */

Test(cpu_handlers, mov_in_stops_the_process_on_a_segmentation_fault)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = PAST_SEG0_END;

  t_instruction instruction = make_instruction("MOV_IN", "EAX", nullptr);
  cr_assert_eq(handler_mov_in(&fx.cpu, fx.context, &instruction, 1), EB_FALSE);
  assert_seg_fault_sent(&fx);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, mov_in_fails_on_an_unknown_segment)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = MISSING_SEGMENT;

  t_instruction instruction = make_instruction("MOV_IN", "EAX", nullptr);
  cr_assert_eq(handler_mov_in(&fx.cpu, fx.context, &instruction, 1), EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, mov_in_fails_when_the_read_fails)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = SEG2_OFFSET(0); /* no stick backs it */

  t_instruction instruction = make_instruction("MOV_IN", "EAX", nullptr);
  cr_assert_eq(handler_mov_in(&fx.cpu, fx.context, &instruction, 1), EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, mov_out_stops_the_process_on_a_segmentation_fault)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->DI = PAST_SEG0_END;

  t_instruction instruction = make_instruction("MOV_OUT", "EAX", nullptr);
  cr_assert_eq(handler_mov_out(&fx.cpu, fx.context, &instruction, 1), EB_FALSE);
  assert_seg_fault_sent(&fx);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, mov_out_fails_on_an_unknown_segment)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->DI = MISSING_SEGMENT;

  t_instruction instruction = make_instruction("MOV_OUT", "EAX", nullptr);
  cr_assert_eq(handler_mov_out(&fx.cpu, fx.context, &instruction, 1), EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, mov_out_fails_when_the_write_fails)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->DI = SEG2_OFFSET(0); /* no stick backs it */

  t_instruction instruction = make_instruction("MOV_OUT", "EAX", nullptr);
  cr_assert_eq(handler_mov_out(&fx.cpu, fx.context, &instruction, 1), EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_copies_the_bytes_from_si_to_di)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = SEG0_OFFSET(4); /* physical 4 */
  fx.context->registers->DI = SEG1_OFFSET(0); /* physical 32 */
  set_register(fx.context->registers, "EAX", 4);

  /* Both stick replies are queued up front: the read, then the write. */
  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "WXY", fx.stick_peer));
  cr_assert(send_string(OP_MEMORY_STICK_WRITE_DONE, "ok", fx.stick_peer));

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1), EB_TRUE);

  cr_assert_eq(receive_op_code(fx.stick_peer), OP_MEMORY_STICK_READ);
  t_list* read = receive_packet(fx.stick_peer);
  cr_assert_eq(*(int*)list_get(read, 0), 4);
  cr_assert_eq(*(int*)list_get(read, 1), 4);
  list_destroy_and_destroy_elements(read, free);

  cr_assert_eq(receive_op_code(fx.stick_peer), OP_MEMORY_STICK_WRITE);
  t_list* write = receive_packet(fx.stick_peer);
  cr_assert_eq(*(int*)list_get(write, 0), 32);
  cr_assert_eq(memcmp(list_get(write, 1), "WXY", 4), 0);
  list_destroy_and_destroy_elements(write, free);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_stops_the_process_when_the_source_faults)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = PAST_SEG0_END;
  fx.context->registers->DI = SEG1_OFFSET(0);

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1),
               EB_FALSE);
  assert_seg_fault_sent(&fx);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_fails_when_the_source_segment_is_unknown)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = MISSING_SEGMENT;
  fx.context->registers->DI = SEG1_OFFSET(0);

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1),
               EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_fails_when_the_destination_segment_is_unknown)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = SEG0_OFFSET(0);
  fx.context->registers->DI = MISSING_SEGMENT;

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1),
               EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_fails_when_the_read_fails)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = SEG2_OFFSET(0); /* no stick backs it */
  fx.context->registers->DI = SEG1_OFFSET(0);
  set_register(fx.context->registers, "EAX", 4);

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1),
               EB_ERROR);

  handler_fixture_end(&fx);
}

Test(cpu_handlers, copy_mem_fails_when_the_write_fails)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  fx.context->registers->SI = SEG0_OFFSET(0);
  fx.context->registers->DI = SEG2_OFFSET(0); /* no stick backs it */
  set_register(fx.context->registers, "EAX", 4);

  cr_assert(send_string(OP_MEMORY_STICK_READ_DONE, "abc", fx.stick_peer));

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", nullptr);
  cr_assert_eq(handler_copy_mem(&fx.cpu, fx.context, &instruction, 1),
               EB_ERROR);

  handler_fixture_end(&fx);
}

/* ── syscalls that cannot reach the Kernel Scheduler ───────────────────── */

Test(cpu_handlers, every_syscall_fails_when_the_scheduler_is_unreachable)
{
  t_handler_fixture fx;
  handler_fixture_start(&fx);
  socket_shutdown(fx.cpu.socket_kernel_scheduler, SOCKET_SHUTDOWN_WRITE);

  struct
  {
    t_handler handler;
    t_instruction instruction;
  } syscalls[] = {
      {handler_mutex_create, make_instruction("MUTEX_CREATE", "M", nullptr)},
      {handler_mutex_lock, make_instruction("MUTEX_LOCK", "M", nullptr)},
      {handler_mutex_unlock, make_instruction("MUTEX_UNLOCK", "M", nullptr)},
      {handler_mem_alloc, make_instruction("MEM_ALLOC", "1", "16")},
      {handler_mem_free, make_instruction("MEM_FREE", "1", nullptr)},
      {handler_sleep, make_instruction("SLEEP", "10", nullptr)},
      {handler_stdout, make_instruction("STDOUT", "EAX", "EBX")},
      {handler_stdin, make_instruction("STDIN", "EAX", "EBX")},
      {handler_init_proc, make_instruction("INIT_PROC", "script", "1")},
      {handler_exit, make_instruction("EXIT", nullptr, nullptr)},
  };

  for (size_t i = 0; i < sizeof(syscalls) / sizeof(syscalls[0]); i++)
  {
    cr_assert_eq(
        syscalls[i].handler(&fx.cpu, fx.context, &syscalls[i].instruction, 1),
        EB_ERROR, "%s should report the failed send",
        syscalls[i].instruction.name);
  }

  handler_fixture_end(&fx);
}

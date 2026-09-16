#include "cpu/handlers.h"

#include <criterion/criterion.h>
#include <unistd.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/syscalls.h"

/* These handlers only touch the context's registers, so the cpu and the pid are
 * placeholders. */

static t_instruction make_instruction(char* name, char* p0, char* p1)
{
  t_instruction instruction = {.name = name, .parameter_count = 0};
  if (p0 != NULL)
  {
    instruction.parameters[0] = p0;
    instruction.parameter_count++;
  }
  if (p1 != NULL)
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

  t_instruction instruction = make_instruction("NOOP", NULL, NULL);
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
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
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
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mem_free_sends_the_syscall_with_a_zero_size)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("MEM_FREE", "3", NULL);
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
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, sleep_sends_the_pid_and_the_blocked_time)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("SLEEP", "250", NULL);
  cr_assert_eq(handler_sleep(&cpu, context, &instruction, 9), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_SLEEP);
  int size;
  t_sleep_request* data = receive_buffer(&size, peer_fd);
  cr_assert_eq(data->pid, 9);
  cr_assert_eq(data->blocked_time_ms, 250);
  free(data);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, stdout_sends_the_address_and_byte_count_from_registers)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
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
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, stdin_sends_the_address_and_byte_count_from_registers)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
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
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_create_sends_the_mutex_name)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction =
      make_instruction("MUTEX_CREATE", "my_mutex", NULL);
  cr_assert_eq(handler_mutex_create(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_CREATE);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_lock_sends_the_mutex_name)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("MUTEX_LOCK", "my_mutex", NULL);
  cr_assert_eq(handler_mutex_lock(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_LOCK);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mutex_unlock_sends_the_mutex_name)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction =
      make_instruction("MUTEX_UNLOCK", "my_mutex", NULL);
  cr_assert_eq(handler_mutex_unlock(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_MUTEX_UNLOCK);
  char* name = receive_string(peer_fd);
  cr_assert_str_eq(name, "my_mutex");
  free(name);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, init_proc_sends_the_script_path_and_priority)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
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
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, exit_sends_the_process_finished_message)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  t_context* context = cpu_make_context();

  t_instruction instruction = make_instruction("EXIT", NULL, NULL);
  cr_assert_eq(handler_exit(&cpu, context, &instruction, 1), EB_FALSE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SYSCALL_EXIT);
  char* message = receive_string(peer_fd);
  cr_assert_str_eq(message, "PROCESS FINISHED");
  free(message);

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

/* ── instructions that touch memory ────────────────────────────────────── */

Test(cpu_handlers, mov_in_reads_a_value_from_memory_into_a_register)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;
  cpu.memory_sticks = list_create();
  int stick_server_fd;
  int stick_client_fd = cpu_connected_pair(&stick_server_fd);
  t_memory_stick_info* stick = cpu_make_stick(0, 100);
  stick->socket_ms = stick_client_fd;
  list_add(cpu.memory_sticks, stick);

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));
  context->registers->SI = 4; /* logical address inside segment 0 */

  uint32_t stored_value = 55;
  cr_assert(send_buffer(OP_MEMORY_STICK_READ_DONE, &stored_value,
                        sizeof(stored_value), stick_server_fd));

  t_instruction instruction = make_instruction("MOV_IN", "EAX", NULL);
  cr_assert_eq(handler_mov_in(&cpu, context, &instruction, 1), EB_TRUE);
  cr_assert_eq(get_register(context->registers, "EAX"), 55);

  cpu_destroy_context(context);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  close(client_fd);
  close(peer_fd);
  close(stick_client_fd);
  close(stick_server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, mov_out_writes_a_register_value_to_memory)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;
  cpu.memory_sticks = list_create();
  int stick_server_fd;
  int stick_client_fd = cpu_connected_pair(&stick_server_fd);
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

  t_instruction instruction = make_instruction("MOV_OUT", "EAX", NULL);
  cr_assert_eq(handler_mov_out(&cpu, context, &instruction, 1), EB_TRUE);

  cr_assert_eq(receive_op_code(stick_server_fd), OP_MEMORY_STICK_WRITE);
  int size;
  void* packet = receive_buffer(&size, stick_server_fd);
  free(packet);

  cpu_destroy_context(context);
  list_destroy_and_destroy_elements(cpu.memory_sticks, free);
  close(client_fd);
  close(peer_fd);
  close(stick_client_fd);
  close(stick_server_fd);
  log_destroy(cpu.logger);
}

Test(cpu_handlers, copy_mem_short_circuits_when_the_destination_faults)
{
  int peer_fd;
  int client_fd = cpu_connected_pair(&peer_fd);
  t_cpu cpu = {.socket_kernel_scheduler = client_fd};
  cpu.logger = cpu_quiet_logger();
  cpu.max_segment_size = 100;

  t_context* context = cpu_make_context();
  list_add(context->segment_table, cpu_make_segment(0, 0, 32));
  context->registers->SI = 4;  /* valid source address */
  context->registers->DI = 90; /* logical address past segment 0's range */
  set_register(context->registers, "EAX", 4);

  t_instruction instruction = make_instruction("COPY_MEM", "EAX", NULL);
  /* dst_addr's mmu() call reports a segmentation fault (segment 0 doesn't
   * cover offset 90), so the handler returns without ever touching src or
   * dst memory. */
  cr_assert_eq(handler_copy_mem(&cpu, context, &instruction, 1), EB_TRUE);

  cr_assert_eq(receive_op_code(peer_fd), OP_SEG_FAULT);
  free(receive_string(peer_fd));

  cpu_destroy_context(context);
  close(client_fd);
  close(peer_fd);
  log_destroy(cpu.logger);
}

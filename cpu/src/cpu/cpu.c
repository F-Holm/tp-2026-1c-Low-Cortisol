#include "cpu/cpu.h"

#include <limits.h>
#include <stdio.h>

#include "cpu/cleanup.h"
#include "cpu/connections.h"
#include "cpu/handlers.h"
#include "cpu/registers.h"
#include "utils/log.h"
#include "utils/string.h"

bool receive_max_segment_size(t_cpu* cpu)
{
  int op_code = receive_op_code(cpu->socket_kernel_memory);
  if (op_code == OP_MAX_SEGMENT_SIZE)
  {
    int size;
    void* buffer = receive_buffer(&size, cpu->socket_kernel_memory);
    cpu->max_segment_size = *(int*)buffer;
    free(buffer);
    log_debug(cpu->logger, "Maximum segment size received: %u",
              cpu->max_segment_size);
  }
  else
  {
    log_error(cpu->logger, "Wrong operation code: %d", op_code);
    int size;
    free(receive_buffer(&size, cpu->socket_kernel_memory));
    return false;
  }
  return true;
}

bool parse_stick_packet(t_cpu* cpu, t_list* packet, char stick_ip[16],
                        char stick_port[6], uint32_t* size)
{
  if (list_size(packet) != 3)
  {
    log_error(cpu->logger,
              "Bad memory stick packet: got %d fields, expected 3 (ip, port, "
              "size)",
              list_size(packet));
    list_destroy_and_destroy_elements(packet, free);
    return false;
  }
  strcpy(stick_ip, list_get(packet, 0));
  strcpy(stick_port, list_get(packet, 1));
  *size = *(int*)list_get(packet, 2);
  list_destroy_and_destroy_elements(packet, free);
  log_debug(cpu->logger, "Memory Stick IP: %s | Port: %s", stick_ip,
            stick_port);
  return true;
}

bool listen_kernel_memory(t_cpu* cpu)
{
  bool keep_going = true;
  while (keep_going)
  {
    int op_code = receive_op_code(cpu->socket_kernel_memory);
    switch (op_code)
    {
      case OP_SEGMENT_TABLE:
        keep_going = false;
        break;

      case OP_SEND_CONTEXT:
        keep_going = false;
        break;

      case OP_SEND_INSTRUCTION:
        keep_going = false;
        break;

      case OP_PACKET:
        if (!connect_memory_stick(cpu))
          return false;
        break;

      case OP_CODE_ERROR:
        log_warning(cpu->logger, "Kernel Memory disconnected");
        return false;

      default:
        log_warning(cpu->logger, "Unrecognized operation code: %d", op_code);
        return false;
    }
  }
  return true;
}

void run_instruction_loop(t_cpu* cpu)
{
  t_context* context = malloc(sizeof(t_context));
  context->segment_changed = false;
  uint32_t pid;
  context->segment_table = list_create();

  while (true)
  {
    pid = receive_pid(cpu);
    if (pid == UINT32_MAX)
      break;

    if (!request_context(cpu, pid))
    {
      log_warning(cpu->logger, "Could not request the context");
      break;
    }
    log_debug(cpu->logger, "Context requested successfully");

    if (listen_kernel_memory(cpu))
      context->registers = receive_context(cpu);
    else
      break;

    if (listen_kernel_memory(cpu))
      context->segment_table = receive_segment_table(cpu, context);
    else
    {
      free(context->registers);
      break;
    }

    if (!run_instruction_cycle(cpu, pid, context))
    {
      free(context->registers);
      break;
    }
    free(context->registers);
  }
  list_destroy_and_destroy_elements(context->segment_table, free);
  free(context);
}

uint32_t receive_pid(t_cpu* cpu)
{
  int op_code = receive_op_code(cpu->socket_kernel_scheduler);
  uint32_t pid = UINT32_MAX;
  if (op_code == OP_RESUME_PROCESS)
  {
    int size;
    void* buffer = receive_buffer(&size, cpu->socket_kernel_scheduler);
    pid = *(uint32_t*)buffer;
    free(buffer);

    log_info(cpu->logger, "PID %u received - starting instruction cycle", pid);
  }
  else if (op_code == 0)
  {
    log_warning(cpu->logger, "Kernel Scheduler disconnected, shutting down");
  }
  else
  {
    log_warning(cpu->logger, "Could not receive the PID - code received: %d",
                op_code);
  }
  return pid;
}

bool request_context(t_cpu* cpu, uint32_t pid)
{
  return send_buffer(OP_REQUEST_CONTEXT, &pid, sizeof(uint32_t),
                     cpu->socket_kernel_memory);
}

t_registers* receive_context(t_cpu* cpu)
{
  int size;
  void* buffer = receive_buffer(&size, cpu->socket_kernel_memory);
  t_registers* registers = malloc(sizeof(t_registers));
  memcpy(registers, buffer, size);
  free(buffer);
  log_debug(cpu->logger, "Context registers received");
  return registers;
}

t_list* receive_segment_table(t_cpu* cpu, t_context* context)
{
  list_destroy_and_destroy_elements(context->segment_table, free);
  log_trace(cpu->logger, "Waiting for the segment table");
  t_list* segment_table = receive_packet(cpu->socket_kernel_memory);
  log_debug(cpu->logger, "Segment table received - segment count: %d",
            list_size(segment_table));
  return segment_table;
}

bool run_instruction_cycle(t_cpu* cpu, uint32_t pid, t_context* context)
{
  bool keep_going = true;
  t_extended_bool syscall = 1;
  t_extended_bool interrupt = 1;
  while (keep_going)
  {
    char* raw_instruction = fetch_stage(cpu, pid, context->registers->PC);

    if (!raw_instruction)
      return false;

    log_info(cpu->logger, "PID: %u - FETCH - Program Counter: %u", pid,
             context->registers->PC);

    t_instruction* instruction = decode_stage(raw_instruction);

    uint32_t initial_pc = context->registers->PC;
    log_info(cpu->logger, "PID: %u - Running: %s", pid, raw_instruction);
    free(raw_instruction);

    syscall = execute_stage(cpu, context, instruction, pid);
    if (syscall == EB_ERROR)
    {
      destroy_instruction(instruction);
      return false;
    }

    if (initial_pc == context->registers->PC)
      context->registers->PC++;

    if (syscall)
    {
      if (!send_string(OP_CPU_CYCLE_OK, "OK", cpu->socket_kernel_scheduler))
      {
        log_warning(cpu->logger, "Could not confirm the end of the cycle");
        destroy_instruction(instruction);
        return false;
      }
    }
    log_trace(cpu->logger, "Kernel Scheduler notified of the end of the cycle");
    interrupt = check_interrupt(cpu, pid);
    if (interrupt == EB_ERROR)
    {
      destroy_instruction(instruction);
      return false;
    }

    if (context->segment_changed && interrupt != EB_NO_TABLE)
    {
      if (!update_segment_table(cpu, pid, context))
      {
        destroy_instruction(instruction);
        return false;
      }

      context->segment_changed = false;
    }

    keep_going = interrupt;

    if (interrupt == EB_NO_TABLE)
    {
      context->segment_changed = false;
      keep_going = false;
    }
    destroy_instruction(instruction);
  }
  return send_updated_context(cpu, pid, context->registers);
}

char* fetch_stage(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  if (!request_instruction(cpu, pid, pc) || !listen_kernel_memory(cpu))
    return NULL;

  return receive_instruction(cpu);
}

bool request_instruction(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  t_packet* packet = create_packet(OP_NEXT_INSTRUCTION);
  packet_append(packet, &pid, sizeof(uint32_t));
  packet_append(packet, &pc, sizeof(uint32_t));
  if (!send_packet(packet, cpu->socket_kernel_memory))
  {
    log_warning(cpu->logger, "Could not request the instruction");
    return false;
  }
  log_trace(cpu->logger, "Instruction requested successfully");
  destroy_packet(packet);
  return true;
}

char* receive_instruction(t_cpu* cpu)
{
  return receive_string(cpu->socket_kernel_memory);
}

t_instruction* decode_stage(char* raw_instruction)
{
  t_instruction* instruction = malloc(sizeof(t_instruction));
  instruction->parameter_count = 0;

  char** parts = string_split(raw_instruction, " ");

  instruction->name = strdup(parts[0]);

  for (int i = 1; parts[i] != NULL; i++)
  {
    instruction->parameters[i - 1] = strdup(parts[i]);
    instruction->parameter_count++;
  }

  for (int i = 0; parts[i] != NULL; i++)
    free(parts[i]);
  free(parts);

  return instruction;
}

t_extended_bool execute_stage(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid)
{
  t_handler handler = dictionary_get(cpu->handlers, instruction->name);

  if (handler == NULL)
  {
    log_error(cpu->logger, "Unknown instruction: %s", instruction->name);
    return EB_ERROR;
  }

  return handler(cpu, context, instruction, pid);
}

t_extended_bool check_interrupt(t_cpu* cpu, uint32_t pid)
{
  int code = receive_op_code(cpu->socket_kernel_scheduler);

  if (code == OP_INTERRUPT)
  {
    log_info(cpu->logger, "Interrupt received");
    char* interrupt_reason = receive_string(cpu->socket_kernel_scheduler);
    log_debug(cpu->logger, "Interrupt reason: %s", interrupt_reason);

    if ((strcmp(interrupt_reason,
                "there is not enough memory for this instruction")) == 0)
    {
      free(interrupt_reason);
      return EB_NO_TABLE;
    }
    free(interrupt_reason);
    return EB_FALSE;
  }
  else if (code == OP_NO_INTERRUPT)
  {
    log_trace(cpu->logger, "No interrupt");
    free(receive_string(cpu->socket_kernel_scheduler));
    return EB_TRUE;
  }
  else if (code == OP_CODE_ERROR)
  {
    log_warning(cpu->logger, "Kernel Scheduler disconnected");
    return EB_ERROR;
  }
  log_warning(cpu->logger, "Unrecognized operation: %d", code);
  return EB_ERROR;
}

bool send_updated_context(t_cpu* cpu, uint32_t pid,
                          t_registers* updated_context)
{
  t_packet* packet = create_packet(OP_UPDATED_CONTEXT);
  packet_append(packet, &pid, sizeof(uint32_t));
  packet_append(packet, updated_context, sizeof(t_registers));
  if (!send_packet(packet, cpu->socket_kernel_memory))
  {
    log_warning(cpu->logger, "Could not send the updated context");
    return false;
  }
  log_debug(cpu->logger, "Updated context sent successfully");
  destroy_packet(packet);
  return true;
}

bool update_segment_table(t_cpu* cpu, uint32_t pid, t_context* context)
{
  log_trace(cpu->logger, "Requesting the updated segment table");
  if (!send_buffer(OP_UPDATED_SEGMENT_TABLE, &pid, sizeof(uint32_t),
                   cpu->socket_kernel_memory))
  {
    log_warning(cpu->logger, "Could not request the segment table update");
    return false;
  }

  if (!listen_kernel_memory(cpu))
    return false;
  context->segment_table = receive_segment_table(cpu, context);
  log_debug(cpu->logger, "Segment table updated successfully");
  return true;
}

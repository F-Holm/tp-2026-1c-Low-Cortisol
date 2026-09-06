#include "cpu/cpu.h"

#include <limits.h>
#include <stdio.h>

#include "cpu/connections.h"
#include "cpu/handlers.h"
#include "cpu/liberacion.h"
#include "cpu/registers.h"
#include "utils/log.h"
#include "utils/string.h"

bool receive_max_segment_size(t_cpu* cpu)
{
  int op_code = recibir_operacion(cpu->socket_kernel_memory);
  if (op_code == OP_TAMANIO_MAX_SEG)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
    cpu->max_segment_size = *(int*)buffer;
    free(buffer);
    log_info(cpu->logger, "Maximum segment size received: %u",
             cpu->max_segment_size);
  }
  else
  {
    log_error(cpu->logger, "## Wrong operation code: %d", op_code);
    int size;
    free(recibir_buffer(&size, cpu->socket_kernel_memory));
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
              "## Bad memory stick packet: expected IP, port and size");
    log_error(cpu->logger, "## size: %d | expected size 3", list_size(packet));
    list_destroy_and_destroy_elements(packet, free);
    return false;
  }
  strcpy(stick_ip, list_get(packet, 0));
  strcpy(stick_port, list_get(packet, 1));
  *size = *(int*)list_get(packet, 2);
  list_destroy_and_destroy_elements(packet, free);
  log_info(cpu->logger, "IP: %s | Port: %s", stick_ip, stick_port);
  return true;
}

bool listen_kernel_memory(t_cpu* cpu)
{
  bool keep_going = true;
  while (keep_going)
  {
    int op_code = recibir_operacion(cpu->socket_kernel_memory);
    switch (op_code)
    {
      case OP_TABLA_DE_SEGMENTOS:
        keep_going = false;
        break;

      case OP_ENVIAR_CONTEXTO:
        keep_going = false;
        break;

      case OP_ENVIAR_INSTRUCCION:
        keep_going = false;
        break;

      case OP_PAQUETE:
        if (!connect_memory_stick(cpu))
          return false;
        break;

      case OP_CODE_ERROR:
        log_error(cpu->logger, "## Kernel Memory disconnected");
        return false;

      default:
        log_error(cpu->logger, "## Unrecognized operation code: %d", op_code);
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
      log_error(cpu->logger, "## Failed to request the context");
      break;
    }
    log_info(cpu->logger, "Context requested successfully");

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
  int op_code = recibir_operacion(cpu->socket_kernel_scheduler);
  uint32_t pid = UINT32_MAX;
  if (op_code == OP_CONTINUAR_PROCESO)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_scheduler);
    pid = *(uint32_t*)buffer;
    free(buffer);

    log_info(cpu->logger, "PID %u received - starting instruction cycle", pid);
  }
  else if (op_code == 0)
  {
    log_info(cpu->logger, "Scheduler disconnected, shutting down");
  }
  else
  {
    log_error(cpu->logger, "## Error receiving the PID - code received: %d",
              op_code);
  }
  return pid;
}

bool request_context(t_cpu* cpu, uint32_t pid)
{
  return enviar_buffer(OP_PEDIR_CONTEXTO, &pid, sizeof(uint32_t),
                       cpu->socket_kernel_memory);
}

t_registros* receive_context(t_cpu* cpu)
{
  int size;
  void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
  t_registros* registers = malloc(sizeof(t_registros));
  memcpy(registers, buffer, size);
  free(buffer);
  log_info(cpu->logger, "Context registers received");
  return registers;
}

t_list* receive_segment_table(t_cpu* cpu, t_context* context)
{
  list_destroy_and_destroy_elements(context->segment_table, free);
  log_info(cpu->logger, "Waiting for the segment table");
  t_list* segment_table = recibir_paquete(cpu->socket_kernel_memory);
  log_info(cpu->logger, "Segment table received - segment count: %d",
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

    log_info(cpu->logger, "## PID: %u - FETCH - Program Counter: %u", pid,
             context->registers->PC);

    t_instruction* instruction = decode_stage(raw_instruction);

    uint32_t initial_pc = context->registers->PC;
    log_info(cpu->logger, "## PID: %u - Running: %s ", pid, raw_instruction);
    free(raw_instruction);

    syscall = execute_stage(cpu, context, instruction, pid);
    if (syscall == EB_ERROR)
    {
      destruir_instruccion(instruction);
      return false;
    }

    if (initial_pc == context->registers->PC)
      context->registers->PC++;

    if (syscall)
    {
      if (!enviar_string(OP_CICLO_CPU_OK, "OK", cpu->socket_kernel_scheduler))
      {
        log_error(cpu->logger, "## Error confirming the end of the cycle");
        destruir_instruccion(instruction);
        return false;
      }
    }
    log_info(cpu->logger, "Scheduler notified of the end of the cycle");
    interrupt = check_interrupt(cpu, pid);
    if (interrupt == EB_ERROR)
    {
      destruir_instruccion(instruction);
      return false;
    }

    if (context->segment_changed && interrupt != EB_NO_TABLE)
    {
      if (!update_segment_table(cpu, pid, context))
      {
        destruir_instruccion(instruction);
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
    destruir_instruccion(instruction);
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
  t_paquete* paquete = crear_paquete(OP_SIGUIENTE_INSTRUCCION);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## Error requesting the instruction");
    return false;
  }
  log_info(cpu->logger, "Instruction requested successfully");
  eliminar_paquete(paquete);
  return true;
}

char* receive_instruction(t_cpu* cpu)
{
  return recibir_string(cpu->socket_kernel_memory);
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
    log_error(cpu->logger, "## Unknown instruction: %s", instruction->name);
    return EB_ERROR;
  }

  return handler(cpu, context, instruction, pid);
}

t_extended_bool check_interrupt(t_cpu* cpu, uint32_t pid)
{
  int code = recibir_operacion(cpu->socket_kernel_scheduler);

  if (code == OP_INTERRUPCION)
  {
    log_info(cpu->logger, "## Interrupt received");
    char* interrupt_reason = recibir_string(cpu->socket_kernel_scheduler);
    log_info(cpu->logger, "Interrupt reason: %s ", interrupt_reason);

    if ((strcmp(interrupt_reason,
                "no hay memoria suficiente para esa instrucción")) == 0)
    {
      free(interrupt_reason);
      return EB_NO_TABLE;
    }
    free(interrupt_reason);
    return EB_FALSE;
  }
  else if (code == OP_SIN_INTERRUPCION)
  {
    log_info(cpu->logger, "No interrupt");
    free(recibir_string(cpu->socket_kernel_scheduler));
    return EB_TRUE;
  }
  else if (code == OP_CODE_ERROR)
  {
    log_info(cpu->logger, "Kernel Scheduler disconnected");
    return EB_ERROR;
  }
  log_error(cpu->logger, "## Unrecognized operation: %d", code);
  return EB_ERROR;
}

bool send_updated_context(t_cpu* cpu, uint32_t pid, t_registros* updated_context)
{
  t_paquete* paquete = crear_paquete(OP_CONTEXTO_ACTUALIZADO);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, updated_context, sizeof(t_registros));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## Error sending the updated context");
    return false;
  }
  log_info(cpu->logger, "Updated context sent successfully");
  eliminar_paquete(paquete);
  return true;
}

bool update_segment_table(t_cpu* cpu, uint32_t pid, t_context* context)
{
  log_info(cpu->logger, "Requesting the updated segment table");
  if (!enviar_buffer(OP_TABLA_SEG_ACTUALIZADA, &pid, sizeof(uint32_t),
                     cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## Failed to request the segment table update");
    return false;
  }

  if (!listen_kernel_memory(cpu))
    return false;
  context->segment_table = receive_segment_table(cpu, context);
  log_info(cpu->logger, "Segment table updated successfully");
  return true;
}

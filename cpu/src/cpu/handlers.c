#include "cpu/handlers.h"

#include <stdio.h>

#include "cpu/cpu.h"
#include "cpu/cleanup.h"
#include "cpu/memory.h"
#include "cpu/registers.h"
#include "utils/log.h"
#include "utils/syscalls.h"

/*             BASIC INSTRUCTIONS HANDLED BY THE CPU           */

t_extended_bool handler_noop(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  return EB_TRUE;
}

t_extended_bool handler_set(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid)
{
  char* reg = instruction->parameters[0];
  uint32_t value = atoi(instruction->parameters[1]);
  set_register(context->registers, reg, value);
  return EB_TRUE;
}

t_extended_bool handler_sum(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid)
{
  char* dest_reg = instruction->parameters[0];
  uint32_t result = get_register(context->registers, dest_reg) +
                    get_register(context->registers, instruction->parameters[1]);

  set_register(context->registers, dest_reg, result);
  return EB_TRUE;
}

t_extended_bool handler_sub(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid)
{
  char* dest_reg = instruction->parameters[0];
  uint32_t result = get_register(context->registers, dest_reg) -
                    get_register(context->registers, instruction->parameters[1]);

  set_register(context->registers, dest_reg, result);
  return EB_TRUE;
}

t_extended_bool handler_jnz(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid)
{
  uint32_t reg_value =
      get_register(context->registers, instruction->parameters[0]);
  if (reg_value != 0)
    set_register(context->registers, "PC", atoi(instruction->parameters[1]));

  return EB_TRUE;
}

/*             INSTRUCTIONS THAT TOUCH MEMORY           */

t_extended_bool handler_mov_in(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid)
{
  uint32_t phys_addr =
      mmu(cpu, context, context->registers->SI, sizeof(uint32_t), pid);

  if (phys_addr == INVALID_ADDRESS)
    return EB_FALSE;
  else if (phys_addr == INVALID_ADDRESS - 1)
    return EB_ERROR;

  void* read_value = read_memory(cpu, phys_addr, sizeof(uint32_t));
  uint32_t value = *(uint32_t*)read_value;
  if (!read_value)
    return EB_ERROR;

  free(read_value);

  set_register(context->registers, instruction->parameters[0], value);

  log_info(cpu->logger,
           "PID: %u - Action: READ - Physical Address: %u - Value: %u", pid,
           phys_addr, value);
  return EB_TRUE;
}

t_extended_bool handler_mov_out(t_cpu* cpu, t_context* context,
                                t_instruction* instruction, uint32_t pid)
{
  uint32_t value = get_register(context->registers, instruction->parameters[0]);
  uint32_t phys_addr =
      mmu(cpu, context, context->registers->DI, sizeof(uint32_t), pid);

  if (phys_addr == INVALID_ADDRESS)
    return EB_FALSE;
  else if (phys_addr == INVALID_ADDRESS - 1)
    return EB_ERROR;

  if (!write_memory(cpu, phys_addr, &value, sizeof(uint32_t)))
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Action: WRITE - Physical Address: %u - Value: %u", pid,
           phys_addr, value);

  return EB_TRUE;
}

t_extended_bool handler_copy_mem(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid)
{
  uint32_t byte_count =
      get_register(context->registers, instruction->parameters[0]);

  uint32_t src_addr =
      mmu(cpu, context, context->registers->SI, sizeof(uint32_t), pid);
  if (src_addr == INVALID_ADDRESS)
    return EB_FALSE;
  else if (src_addr == INVALID_ADDRESS - 1)
    return EB_ERROR;

  uint32_t dst_addr =
      mmu(cpu, context, context->registers->DI, sizeof(uint32_t), pid);
  if (dst_addr == INVALID_ADDRESS)
    return EB_TRUE;
  else if (dst_addr == INVALID_ADDRESS - 1)
    return EB_ERROR;

  void* read_bytes = read_memory(cpu, src_addr, byte_count);

  if (!read_bytes)
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Action: READ - Physical Address: %u - Value: %s", pid,
           src_addr, (char*)read_bytes);

  if (!write_memory(cpu, dst_addr, read_bytes, byte_count))
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Action: WRITE - Physical Address: %u - Value: %s", pid,
           dst_addr, (char*)read_bytes);

  free(read_bytes);
  return EB_TRUE;
}

/*             SYSCALLS (HANDLED BY THE SCHEDULER)           */

t_extended_bool handler_mutex_create(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_CREATE, instruction->parameters[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_mutex_lock(t_cpu* cpu, t_context* context,
                                   t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_LOCK, instruction->parameters[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_mutex_unlock(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_UNLOCK, instruction->parameters[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_mem_alloc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid)
{
  t_syscall_memory* syscall_data;
  syscall_data = malloc(sizeof(t_syscall_memory));

  syscall_data->pid = pid;
  syscall_data->id_segmento = atoi(instruction->parameters[0]);
  syscall_data->tamanio = atoi(instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_MEM_ALLOC, syscall_data, sizeof(t_syscall_memory),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    free(syscall_data);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    free(syscall_data);
    return EB_ERROR;
  }
  context->segment_changed = EB_TRUE;
  return EB_FALSE;
}

t_extended_bool handler_mem_free(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid)
{
  t_syscall_memory* syscall_data;
  syscall_data = malloc(sizeof(t_syscall_memory));

  syscall_data->pid = pid;
  syscall_data->id_segmento = atoi(instruction->parameters[0]);
  syscall_data->tamanio = 0;

  if (enviar_buffer(OP_SYSCALL_MEM_FREE, syscall_data, sizeof(t_syscall_memory),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    free(syscall_data);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    free(syscall_data);
    return EB_ERROR;
  }
  context->segment_changed = EB_TRUE;
  return EB_FALSE;
}

t_extended_bool handler_sleep(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid)
{
  t_peticion_sleep* syscall_data;
  syscall_data = malloc(sizeof(t_peticion_sleep));

  syscall_data->pid = pid;
  syscall_data->tiempo_bloqueado = atoi(instruction->parameters[0]);

  if (enviar_buffer(OP_SYSCALL_SLEEP, syscall_data, sizeof(t_peticion_sleep),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    free(syscall_data);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    free(syscall_data);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_stdout(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid)
{
  t_peticion_stdout* syscall_data;
  syscall_data = malloc(sizeof(t_peticion_stdout));

  syscall_data->pid = pid;
  syscall_data->direccion_logica =
      get_register(context->registers, instruction->parameters[0]);
  syscall_data->tamanio_a_escribir =
      get_register(context->registers, instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_STDOUT, syscall_data, sizeof(t_peticion_stdout),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    free(syscall_data);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    free(syscall_data);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_stdin(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid)
{
  t_peticion_stdin* syscall_data;
  syscall_data = malloc(sizeof(t_peticion_stdin));

  syscall_data->pid = pid;
  syscall_data->direccion_logica =
      get_register(context->registers, instruction->parameters[0]);
  syscall_data->tamanio_a_leer =
      get_register(context->registers, instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_STDIN, syscall_data, sizeof(t_peticion_stdin),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    free(syscall_data);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    free(syscall_data);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_init_proc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid)
{
  int priority = atoi(instruction->parameters[1]);

  t_paquete* syscall_packet = crear_paquete(OP_SYSCALL_INIT_PROC);
  agregar_string_a_paquete(syscall_packet, instruction->parameters[0]);
  agregar_a_paquete(syscall_packet, &priority, sizeof(int));

  if (enviar_paquete(syscall_packet, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
    eliminar_paquete(syscall_packet);
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    eliminar_paquete(syscall_packet);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_exit(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_EXIT, "PROCESS FINISHED",
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall sent to the kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the syscall to the kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}

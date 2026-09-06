#include "cpu/handlers.h"

#include <stdio.h>

#include "cpu/cpu.h"
#include "cpu/liberacion.h"
#include "cpu/memoria.h"
#include "cpu/registers.h"
#include "utils/syscalls.h"
#include "utils/log.h"

/*             INSTRUCCIONES BASICAS MANEJADAS POR CPU           */

t_extended_bool handler_noop(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid)
{
  return EB_TRUE;
}

t_extended_bool handler_set(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  char* registro = instruction->parameters[0];
  uint32_t valor = atoi(instruction->parameters[1]);
  set_register(context->registers, registro, valor);
  return EB_TRUE;
}

t_extended_bool handler_sum(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  char* registro_destino = instruction->parameters[0];
  uint32_t resultado =
      get_register(context->registers, registro_destino) +
      get_register(context->registers, instruction->parameters[1]);

  set_register(context->registers, registro_destino, resultado);
  return EB_TRUE;
}

t_extended_bool handler_sub(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  char* registro_destino = instruction->parameters[0];
  uint32_t resultado =
      get_register(context->registers, registro_destino) -
      get_register(context->registers, instruction->parameters[1]);

  set_register(context->registers, registro_destino, resultado);
  return EB_TRUE;
}

t_extended_bool handler_jnz(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid)
{
  uint32_t valor_registro =
      get_register(context->registers, instruction->parameters[0]);
  if (valor_registro != 0)
    set_register(context->registers, "PC", atoi(instruction->parameters[1]));

  return EB_TRUE;
}

/*             INSTRUCCIONES CON MODIFICACION DE MEMORIA           */

t_extended_bool handler_mov_in(t_cpu* cpu, t_context* context,
                                t_instruction* instruction, uint32_t pid)
{
  uint32_t dir_fisica =
      mmu(cpu, context, context->registers->SI, sizeof(uint32_t), pid);

  if (dir_fisica == DIR_INVALIDA)
    return EB_FALSE;
  else if (dir_fisica == DIR_INVALIDA - 1)
    return EB_ERROR;

  void* dato_leido = leer_memoria(cpu, dir_fisica, sizeof(uint32_t));
  uint32_t valor = *(uint32_t*)dato_leido;
  if (!dato_leido)
    return EB_ERROR;

  free(dato_leido);

  set_register(context->registers, instruction->parameters[0], valor);

  log_info(cpu->logger,
           "PID: %u - Acción: LEER - Dirección Física: %u - Valor: %u", pid,
           dir_fisica, valor);
  return EB_TRUE;
}

t_extended_bool handler_mov_out(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid)
{
  uint32_t valor =
      get_register(context->registers, instruction->parameters[0]);
  uint32_t dir_fisica =
      mmu(cpu, context, context->registers->DI, sizeof(uint32_t), pid);

  if (dir_fisica == DIR_INVALIDA)
    return EB_FALSE;
  else if (dir_fisica == DIR_INVALIDA - 1)
    return EB_ERROR;

  if (!escribir_memoria(cpu, dir_fisica, &valor, sizeof(uint32_t)))
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Acción: ESCRIBIR - Dirección Física: %u - Valor: %u", pid,
           dir_fisica, valor);

  return EB_TRUE;
}

t_extended_bool handler_copy_mem(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid)
{
  uint32_t cant_bytes =
      get_register(context->registers, instruction->parameters[0]);

  uint32_t direccion_SI =
      mmu(cpu, context, context->registers->SI, sizeof(uint32_t), pid);
  if (direccion_SI == DIR_INVALIDA)
    return EB_FALSE;
  else if (direccion_SI == DIR_INVALIDA - 1)
    return EB_ERROR;

  uint32_t direccion_DI =
      mmu(cpu, context, context->registers->DI, sizeof(uint32_t), pid);
  if (direccion_DI == DIR_INVALIDA)
    return EB_TRUE;
  else if (direccion_DI == DIR_INVALIDA - 1)
    return EB_ERROR;

  void* bytes_leidos = leer_memoria(cpu, direccion_SI, cant_bytes);

  if (!bytes_leidos)
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Acción: LEER - Dirección Física: %u - Valor: %s", pid,
           direccion_SI, (char*)bytes_leidos);

  if (!escribir_memoria(cpu, direccion_DI, bytes_leidos, cant_bytes))
    return EB_ERROR;

  log_info(cpu->logger,
           "PID: %u - Acción: ESCRIBIR - Dirección Física: %u - Valor: %s", pid,
           direccion_DI, (char*)bytes_leidos);

  free(bytes_leidos);
  return EB_TRUE;
}

/*             SYSCALLS(MANEJADAS POR SCHEDULER)           */

t_extended_bool handler_mutex_create(t_cpu* cpu, t_context* context,
                                      t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_CREATE, instruction->parameters[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
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
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
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
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_mem_alloc(t_cpu* cpu, t_context* context,
                                   t_instruction* instruction, uint32_t pid)
{
  t_syscall_memory* datos_syscall;
  datos_syscall = malloc(sizeof(t_syscall_memory));

  datos_syscall->pid = pid;
  datos_syscall->id_segmento = atoi(instruction->parameters[0]);
  datos_syscall->tamanio = atoi(instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_MEM_ALLOC, datos_syscall,
                    sizeof(t_syscall_memory), cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    free(datos_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    free(datos_syscall);
    return EB_ERROR;
  }
  context->segment_changed = EB_TRUE;
  return EB_FALSE;
}

t_extended_bool handler_mem_free(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid)
{
  t_syscall_memory* datos_syscall;
  datos_syscall = malloc(sizeof(t_syscall_memory));

  datos_syscall->pid = pid;
  datos_syscall->id_segmento = atoi(instruction->parameters[0]);
  datos_syscall->tamanio = 0;

  if (enviar_buffer(OP_SYSCALL_MEM_FREE, datos_syscall,
                    sizeof(t_syscall_memory), cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    free(datos_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    free(datos_syscall);
    return EB_ERROR;
  }
  context->segment_changed = EB_TRUE;
  return EB_FALSE;
}

t_extended_bool handler_sleep(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid)
{
  t_peticion_sleep* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_sleep));

  datos_syscall->pid = pid;
  datos_syscall->tiempo_bloqueado = atoi(instruction->parameters[0]);

  if (enviar_buffer(OP_SYSCALL_SLEEP, datos_syscall, sizeof(t_peticion_sleep),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    free(datos_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    free(datos_syscall);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_stdout(t_cpu* cpu, t_context* context,
                                t_instruction* instruction, uint32_t pid)
{
  t_peticion_stdout* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_stdout));

  datos_syscall->pid = pid;
  datos_syscall->direccion_logica =
      get_register(context->registers, instruction->parameters[0]);
  datos_syscall->tamanio_a_escribir =
      get_register(context->registers, instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_STDOUT, datos_syscall, sizeof(t_peticion_stdout),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    free(datos_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    free(datos_syscall);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_stdin(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid)
{
  t_peticion_stdin* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_stdin));

  datos_syscall->pid = pid;
  datos_syscall->direccion_logica =
      get_register(context->registers, instruction->parameters[0]);
  datos_syscall->tamanio_a_leer =
      get_register(context->registers, instruction->parameters[1]);

  if (enviar_buffer(OP_SYSCALL_STDIN, datos_syscall, sizeof(t_peticion_stdin),
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    free(datos_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    free(datos_syscall);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_init_proc(t_cpu* cpu, t_context* context,
                                   t_instruction* instruction, uint32_t pid)
{
  int prioridad = atoi(instruction->parameters[1]);

  t_paquete* paquete_syscall = crear_paquete(OP_SYSCALL_INIT_PROC);
  agregar_string_a_paquete(paquete_syscall, instruction->parameters[0]);
  agregar_a_paquete(paquete_syscall, &prioridad, sizeof(int));

  if (enviar_paquete(paquete_syscall, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
    eliminar_paquete(paquete_syscall);
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    eliminar_paquete(paquete_syscall);
    return EB_ERROR;
  }
  return EB_FALSE;
}

t_extended_bool handler_exit(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_EXIT, "PROCESO TERMINADO",
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    return EB_ERROR;
  }
  return EB_FALSE;
}
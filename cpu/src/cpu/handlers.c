#include "cpu/handlers.h"

#include <commons/log.h>
#include <stdio.h>

#include "cpu/cpu.h"
#include "cpu/liberacion.h"
#include "cpu/memoria.h"
#include "cpu/registros.h"
#include "utils/kernel_scheduler_cpu.h"

/*             INSTRUCCIONES BASICAS MANEJADAS POR CPU           */

bool handler_noop(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                  uint32_t pid)
{
  return true;
}

bool handler_set(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid)
{
  char* registro = instruccion->parametros[0];
  uint32_t valor = atoi(instruccion->parametros[1]);
  set_registro(contexto->registros, registro, valor);
  return true;
}

bool handler_sum(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid)
{
  char* registro_destino = instruccion->parametros[0];
  uint32_t resultado =
      get_registro(contexto->registros, registro_destino) +
      get_registro(contexto->registros, instruccion->parametros[1]);

  set_registro(contexto->registros, registro_destino, resultado);
  return true;
}

bool handler_sub(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid)
{
  char* registro_destino = instruccion->parametros[0];
  uint32_t resultado =
      get_registro(contexto->registros, registro_destino) -
      get_registro(contexto->registros, instruccion->parametros[1]);

  set_registro(contexto->registros, registro_destino, resultado);
  return true;
}

bool handler_jnz(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid)
{
  uint32_t valor_registro =
      get_registro(contexto->registros, instruccion->parametros[0]);
  if (valor_registro != 0)
    set_registro(contexto->registros, "PC", atoi(instruccion->parametros[1]));

  return true;
}

/*             INSTRUCCIONES CON MODIFICACION DE MEMORIA           */

t_bool_extendido handler_mov_in(t_cpu* cpu, t_contexto* contexto,
                    t_instruccion* instruccion, uint32_t pid)
{
  uint32_t dir_fisica =
      mmu(cpu, contexto, contexto->registros->SI, sizeof(uint32_t), pid);

  if (dir_fisica == DIR_INVALIDA)
    return BE_TRUE;
  else if(dir_fisica == DIR_INVALIDA-1)
    return BE_ERROR;


  void* dato_leido = leer_memoria(cpu, dir_fisica, sizeof(uint32_t));
  uint32_t valor = *(uint32_t*)dato_leido;
  free(dato_leido);

  set_registro(contexto->registros, instruccion->parametros[0], valor);

  log_info(cpu->logger,
           "PID: %u - Acción: LEER - Dirección Física: %u - Valor: %u", pid,
           dir_fisica, valor);
  return BE_TRUE;
}

t_bool_extendido handler_mov_out(t_cpu* cpu, t_contexto* contexto,
                     t_instruccion* instruccion, uint32_t pid)
{
  uint32_t valor =
      get_registro(contexto->registros, instruccion->parametros[0]);
  uint32_t dir_fisica =
      mmu(cpu, contexto, contexto->registros->DI, sizeof(uint32_t), pid);

  if (dir_fisica == DIR_INVALIDA)
    return BE_TRUE;
  else if(dir_fisica == DIR_INVALIDA-1)
    return BE_ERROR;

  escribir_memoria(cpu, dir_fisica, &valor, sizeof(uint32_t));

  log_info(cpu->logger,
           "PID: %u - Acción: ESCRIBIR - Dirección Física: %u - Valor: %u", pid,
           dir_fisica, valor);

  return BE_TRUE;
}

t_bool_extendido handler_copy_mem(t_cpu* cpu, t_contexto* contexto,
                      t_instruccion* instruccion, uint32_t pid)
{
  uint32_t cant_bytes =
      get_registro(contexto->registros, instruccion->parametros[0]);

  uint32_t direccion_SI =
      mmu(cpu, contexto, contexto->registros->SI, sizeof(uint32_t), pid);
  if (direccion_SI == DIR_INVALIDA)
    return BE_TRUE;
  else if(direccion_SI == DIR_INVALIDA-1)
    return BE_ERROR;

  uint32_t direccion_DI =
      mmu(cpu, contexto, contexto->registros->DI, sizeof(uint32_t), pid);
  if (direccion_DI == DIR_INVALIDA)
    return BE_TRUE;
  else if(direccion_DI == DIR_INVALIDA-1)
    return BE_ERROR;

  void* bytes_leidos = leer_memoria(cpu, direccion_SI, cant_bytes);

  log_info(cpu->logger,
           "PID: %u - Acción: LEER - Dirección Física: %u - Valor: %s", pid,
           direccion_SI, (char*)bytes_leidos);

  escribir_memoria(cpu, direccion_DI, bytes_leidos, cant_bytes);

  log_info(cpu->logger,
           "PID: %u - Acción: ESCRIBIR - Dirección Física: %u - Valor: %s", pid,
           direccion_DI, (char*)bytes_leidos);

  free(bytes_leidos);
  return BE_TRUE;
}

/*             SYSCALLS(MANEJADAS POR SCHEDULER)           */

t_bool_extendido handler_mutex_create(t_cpu* cpu, t_contexto* contexto,
                          t_instruccion* instruccion, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_CREATE, instruccion->parametros[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_mutex_lock(t_cpu* cpu, t_contexto* contexto,
                        t_instruccion* instruccion, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_LOCK, instruccion->parametros[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_mutex_unlock(t_cpu* cpu, t_contexto* contexto,
                          t_instruccion* instruccion, uint32_t pid)
{
  if (enviar_string(OP_SYSCALL_MUTEX_UNLOCK, instruccion->parametros[0],
                    cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "Syscall correctamente enviada al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo el envio de la syscall al kernel scheduler");
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_mem_alloc(t_cpu* cpu, t_contexto* contexto,
                       t_instruccion* instruccion, uint32_t pid)
{
  t_syscall_memory* datos_syscall;
  datos_syscall = malloc(sizeof(t_syscall_memory));

  datos_syscall->pid = pid;
  datos_syscall->id_segmento = atoi(instruccion->parametros[0]);
  datos_syscall->tamanio = atoi(instruccion->parametros[1]);

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
    return BE_ERROR;
  }
  contexto->cambio_segmento = true;
  return BE_FALSE;
}

t_bool_extendido handler_mem_free(t_cpu* cpu, t_contexto* contexto,
                      t_instruccion* instruccion, uint32_t pid)
{
  t_syscall_memory* datos_syscall;
  datos_syscall = malloc(sizeof(t_syscall_memory));

  datos_syscall->pid = pid;
  datos_syscall->id_segmento = atoi(instruccion->parametros[0]);
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
    return BE_ERROR;
  }
  contexto->cambio_segmento = true;
  return BE_FALSE;
}

t_bool_extendido handler_sleep(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid)
{
  t_peticion_sleep* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_sleep));

  datos_syscall->pid = pid;
  datos_syscall->tiempo_bloqueado = atoi(instruccion->parametros[0]);

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
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_stdout(t_cpu* cpu, t_contexto* contexto,
                    t_instruccion* instruccion, uint32_t pid)
{
  t_peticion_stdout* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_stdout));

  datos_syscall->pid = pid;
  datos_syscall->direccion_logica =
      get_registro(contexto->registros, instruccion->parametros[0]);
  datos_syscall->tamanio_a_escribir =
      get_registro(contexto->registros, instruccion->parametros[1]);

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
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_stdin(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid)
{
  t_peticion_stdin* datos_syscall;
  datos_syscall = malloc(sizeof(t_peticion_stdin));

  datos_syscall->pid = pid;
  datos_syscall->direccion_logica =
      get_registro(contexto->registros, instruccion->parametros[0]);
  datos_syscall->tamanio_a_leer =
      get_registro(contexto->registros, instruccion->parametros[1]);

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
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_init_proc(t_cpu* cpu, t_contexto* contexto,
                       t_instruccion* instruccion, uint32_t pid)
{
  int prioridad = atoi(instruccion->parametros[1]);

  t_paquete* paquete_syscall = crear_paquete(OP_SYSCALL_INIT_PROC);
  agregar_string_a_paquete(paquete_syscall, instruccion->parametros[0]);
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
    return BE_ERROR;
  }
  return BE_FALSE;
}

t_bool_extendido handler_exit(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                  uint32_t pid)
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
    return BE_ERROR;
  }
  return BE_FALSE;
}
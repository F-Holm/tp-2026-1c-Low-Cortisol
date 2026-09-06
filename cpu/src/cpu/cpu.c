#include "cpu/cpu.h"

#include <limits.h>
#include <stdio.h>

#include "cpu/conexiones.h"
#include "cpu/handlers.h"
#include "cpu/liberacion.h"
#include "cpu/registers.h"
#include "utils/log.h"
#include "utils/string.h"

bool recibir_tamanio_maximo_segmento(t_cpu* cpu)
{
  int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);
  if (codigo_operacion == OP_TAMANIO_MAX_SEG)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
    cpu->tamanio_max_segmento = *(int*)buffer;
    free(buffer);
    log_info(cpu->logger, "Tamaño máximo de segmento recibido: %u",
             cpu->tamanio_max_segmento);
  }
  else
  {
    log_error(cpu->logger, "## Código de operación erroneo: %d",
              codigo_operacion);
    int size;
    free(recibir_buffer(&size, cpu->socket_kernel_memory));
    return false;
  }
  return true;
}

bool manejar_paquete(t_cpu* cpu, t_list* lista_paquete, char ip_stick[16],
                     char puerto_stick[6], uint32_t* tamanio)
{
  if (list_size(lista_paquete) != 3)
  {
    log_error(
        cpu->logger,
        "## Error en la recepción de la IP ,puerto y tamaño del Memory stick");
    log_error(cpu->logger, "## size: %d | expected size 3",
              list_size(lista_paquete));
    list_destroy_and_destroy_elements(lista_paquete, free);
    return false;
  }
  strcpy(ip_stick, list_get(lista_paquete, 0));
  strcpy(puerto_stick, list_get(lista_paquete, 1));
  *tamanio = *(int*)list_get(lista_paquete, 2);
  list_destroy_and_destroy_elements(lista_paquete, free);
  log_info(cpu->logger, "IP: %s | Puerto: %s", ip_stick, puerto_stick);
  return true;
}

bool escuchar_kernel_memory(t_cpu* cpu)
{
  bool seguir = true;
  while (seguir)
  {
    int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);
    switch (codigo_operacion)
    {
      case OP_TABLA_DE_SEGMENTOS:
        seguir = false;
        break;

      case OP_ENVIAR_CONTEXTO:
        seguir = false;
        break;

      case OP_ENVIAR_INSTRUCCION:
        seguir = false;
        break;

      case OP_PAQUETE:
        if (!conectar_memory_stick(cpu))
          return false;
        break;

      case OP_CODE_ERROR:
        log_error(cpu->logger, "## Kernel Memory desconectado");
        return false;

      default:
        log_error(cpu->logger, "## Codigo de operacion no reconocido: %d",
                  codigo_operacion);
        return false;
    }
  }
  return true;
}

void manejo_instrucciones(t_cpu* cpu)
{
  t_context* context = malloc(sizeof(t_context));
  context->segment_changed = false;
  uint32_t pid;
  context->segment_table = list_create();

  while (true)
  {
    pid = recibir_pid_kernel_scheduler(cpu);
    if (pid == UINT32_MAX)
      break;

    if (!pedir_contexto_kernel_memory(cpu, pid))
    {
      log_error(cpu->logger, "## Fallo en la petición del context");
      break;
    }
    log_info(cpu->logger, "context pedido correctamente");

    if (escuchar_kernel_memory(cpu))
      context->registers = recibir_contexto_kernel_memory(cpu);
    else
      break;

    if (escuchar_kernel_memory(cpu))
      context->segment_table = recibir_tabla_segmentos(cpu, context);
    else
    {
      free(context->registers);
      break;
    }

    if (!ejecutar_ciclo_instruccion(cpu, pid, context))
    {
      free(context->registers);
      break;
    }
    free(context->registers);
  }
  list_destroy_and_destroy_elements(context->segment_table, free);
  free(context);
}

uint32_t recibir_pid_kernel_scheduler(t_cpu* cpu)
{
  int codigo_operacion = recibir_operacion(cpu->socket_kernel_scheduler);
  uint32_t pid = UINT32_MAX;
  if (codigo_operacion == OP_CONTINUAR_PROCESO)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_scheduler);
    pid = *(uint32_t*)buffer;
    free(buffer);

    log_info(cpu->logger, "PID recibido: %u - Iniciando ciclo de instrucción",
             pid);
  }
  else if (codigo_operacion == 0)
  {
    log_info(cpu->logger, "Scheduler desconectado, cerrando modulo");
  }
  else
  {
    log_error(cpu->logger, "## Error al recibir el PID - Codigo recibido: %d",
              codigo_operacion);
  }
  return pid;
}

bool pedir_contexto_kernel_memory(t_cpu* cpu, uint32_t pid)
{
  return enviar_buffer(OP_PEDIR_CONTEXTO, &pid, sizeof(uint32_t),
                       cpu->socket_kernel_memory);
}

t_registros* recibir_contexto_kernel_memory(t_cpu* cpu)
{
  int size;
  void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
  t_registros* registros = malloc(sizeof(t_registros));
  memcpy(registros, buffer, size);
  free(buffer);
  log_info(cpu->logger, "Registros del context recibidos");
  return registros;
}

t_list* recibir_tabla_segmentos(t_cpu* cpu, t_context* context)
{
  list_destroy_and_destroy_elements(context->segment_table, free);
  log_info(cpu->logger, "Esperando tabla de segmentos");
  t_list* tabla_segmentos = recibir_paquete(cpu->socket_kernel_memory);
  log_info(cpu->logger,
           "Tabla de segmentos recibida - Cantidad de segmentos: %d",
           list_size(tabla_segmentos));
  return tabla_segmentos;
}

bool ejecutar_ciclo_instruccion(t_cpu* cpu, uint32_t pid, t_context* context)
{
  bool seguir = true;
  t_bool_extendido syscall = 1;
  t_bool_extendido interrupt = 1;
  while (seguir)
  {
    char* instruccion_KM = etapa_fetch(cpu, pid, context->registers->PC);

    if (!instruccion_KM)
      return false;

    log_info(cpu->logger, "## PID: %u - FETCH - Program Counter: %u", pid,
             context->registers->PC);

    t_instruccion* instruccion = etapa_decode(instruccion_KM);

    uint32_t pc_inicial = context->registers->PC;
    log_info(cpu->logger, "## PID: %u - Ejecutando: %s ", pid, instruccion_KM);
    free(instruccion_KM);

    syscall = etapa_execute(cpu, context, instruccion, pid);
    if (syscall == BE_ERROR)
    {
      destruir_instruccion(instruccion);
      return false;
    }

    if (pc_inicial == context->registers->PC)
      context->registers->PC++;

    if (syscall)
    {
      if (!enviar_string(OP_CICLO_CPU_OK, "OK", cpu->socket_kernel_scheduler))
      {
        log_error(cpu->logger, "## Error en la confirmación del fin de ciclo");
        destruir_instruccion(instruccion);
        return false;
      }
    }
    log_info(cpu->logger, "Scheduler notificado del fin de ciclo");
    interrupt = check_interrupt(cpu, pid);
    if (interrupt == BE_ERROR)
    {
      destruir_instruccion(instruccion);
      return false;
    }

    if (context->segment_changed && interrupt != BE_SIN_TABLA)
    {
      if (!actualizar_tabla_segmentos(cpu, pid, context))
      {
        destruir_instruccion(instruccion);
        return false;
      }

      context->segment_changed = false;
    }

    seguir = interrupt;

    if (interrupt == BE_SIN_TABLA)
    {
      context->segment_changed = false;
      seguir = false;
    }
    destruir_instruccion(instruccion);
  }
  return enviar_contexto_actualizado(cpu, pid, context->registers);
}

char* etapa_fetch(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  if (!pedir_instruccion_kernel_memory(cpu, pid, pc) ||
      !escuchar_kernel_memory(cpu))
    return NULL;

  return recibir_instruccion_kernel_memory(cpu);
}

bool pedir_instruccion_kernel_memory(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  t_paquete* paquete = crear_paquete(OP_SIGUIENTE_INSTRUCCION);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## error en la petición de la instrucción");
    return false;
  }
  log_info(cpu->logger, "Instrucción pedida correctamente");
  eliminar_paquete(paquete);
  return true;
}

char* recibir_instruccion_kernel_memory(t_cpu* cpu)
{
  return recibir_string(cpu->socket_kernel_memory);
}

t_instruccion* etapa_decode(char* instruccion_KM)
{
  t_instruccion* instruccion = malloc(sizeof(t_instruccion));
  instruccion->cantidad_parametros = 0;

  char** partes = string_split(instruccion_KM, " ");

  instruccion->nombre = strdup(partes[0]);

  for (int i = 1; partes[i] != NULL; i++)
  {
    instruccion->parametros[i - 1] = strdup(partes[i]);
    instruccion->cantidad_parametros++;
  }

  // libero instriccion
  for (int i = 0; partes[i] != NULL; i++)
    free(partes[i]);
  free(partes);

  return instruccion;
}

t_bool_extendido etapa_execute(t_cpu* cpu, t_context* context,
                               t_instruccion* instruccion, uint32_t pid)
{
  t_handler handler = dictionary_get(cpu->handlers, instruccion->nombre);

  if (handler == NULL)
  {
    log_error(cpu->logger, "## instruccion desconocida: %s",
              instruccion->nombre);
    return BE_ERROR;
  }

  return handler(cpu, context, instruccion, pid);
}

t_bool_extendido check_interrupt(t_cpu* cpu, uint32_t pid)
{
  int codigo = recibir_operacion(cpu->socket_kernel_scheduler);

  if (codigo == OP_INTERRUPCION)
  {
    log_info(cpu->logger, "## Interrupción recibida");
    char* interrucpcion_recibida = recibir_string(cpu->socket_kernel_scheduler);
    log_info(cpu->logger, "Razon de la interrupcion: %s ",
             interrucpcion_recibida);

    if ((strcmp(interrucpcion_recibida,
                "no hay memoria suficiente para esa instrucción")) == 0)
    {
      free(interrucpcion_recibida);
      return BE_SIN_TABLA;
    }
    free(interrucpcion_recibida);
    return BE_FALSE;
  }
  else if (codigo == OP_SIN_INTERRUPCION)
  {
    log_info(cpu->logger, "Sin interrupción");
    free(recibir_string(cpu->socket_kernel_scheduler));
    return BE_TRUE;
  }
  else if (codigo == OP_CODE_ERROR)
  {
    log_info(cpu->logger, "Kernel Scheduler desconectado");
    return BE_ERROR;
  }
  log_error(cpu->logger, "## Operacion no reconocida: %d", codigo);
  return BE_ERROR;
}

bool enviar_contexto_actualizado(t_cpu* cpu, uint32_t pid,
                                 t_registros* contexto_actualizado)
{
  t_paquete* paquete = crear_paquete(OP_CONTEXTO_ACTUALIZADO);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, contexto_actualizado, sizeof(t_registros));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## error en el envio del context actualizado");
    return false;
  }
  log_info(cpu->logger, "Envio correcto del context actualizado");
  eliminar_paquete(paquete);
  return true;
}

bool actualizar_tabla_segmentos(t_cpu* cpu, uint32_t pid, t_context* context)
{
  log_info(cpu->logger, "Pidiendo la tabla de segmentos actualizada");
  if (!enviar_buffer(OP_TABLA_SEG_ACTUALIZADA, &pid, sizeof(uint32_t),
                     cpu->socket_kernel_memory))
  {
    log_error(cpu->logger,
              "## Fallo en la petición para actualizar tabla de segmentos");
    return false;
  }

  if (!escuchar_kernel_memory(cpu))
    return false;
  context->segment_table = recibir_tabla_segmentos(cpu, context);
  log_info(cpu->logger, "Tabla actualizada correctamente");
  return true;
}

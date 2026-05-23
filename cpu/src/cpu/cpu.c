#include "cpu/cpu.h"

#include <commons/log.h>
#include <commons/string.h>
#include <stdio.h>

#include "cpu/conexiones.h"
#include "cpu/handlers.h"
#include "cpu/liberacion.h"
#include "utils/kernel_memory_cpu.h"

bool manejar_paquete(t_cpu* cpu, t_list* lista_paquete, char ip_stick[16],
                     char puerto_stick[6])
{
  if (list_size(lista_paquete) != 2)
  {
    log_error(cpu->logger,
              "## Error en la recepción de la IP y puerto del Memory stick");
    log_error(cpu->logger, "## size: %d | expected size 2",
              list_size(lista_paquete));
    list_destroy_and_destroy_elements(lista_paquete, free);
    return false;
  }
  strcpy(ip_stick, list_get(lista_paquete, 0));
  strcpy(puerto_stick, list_get(lista_paquete, 1));
  list_destroy_and_destroy_elements(lista_paquete, free);
  log_info(cpu->logger, "IP: %s | Puerto: %s", ip_stick, puerto_stick);
  return true;
}

void escuchar_kernel_memory(t_cpu* cpu)
{
  bool seguir = true;
  while (seguir)
  {
    int codigo_operacion = recibir_operacion(cpu->socket_kernel_memory);
    switch (codigo_operacion)
    {
      case OP_ENVIAR_CONTEXTO:
        seguir = false;
        break;

      case OP_ENVIAR_INSTRUCCION:
        seguir = false;
        break;

      case OP_PAQUETE:
        if (!conectar_memory_stick(cpu))
          cerrar_modulo(cpu);
        break;

      default:
        cerrar_modulo(cpu);
        break;
    }
  }
}

void manejo_instrucciones(t_cpu* cpu)
{
  t_contexto* contexto;
  uint32_t pid;

  while (true)
  {
    pid = recibir_pid_kernel_scheduler(cpu);

    if (!pedir_contexto_kernel_memory(cpu, pid))
    {
      log_error(cpu->logger, "## Fallo en la petición del contexto");
      break;
    }
    log_info(cpu->logger, "contexto pedido correctamente");

    contexto = recibir_contexto_kernel_memory(cpu);

    ejecutar_ciclo_instruccion(cpu, pid, contexto);
  }
  cerrar_modulo(cpu);
}

uint32_t recibir_pid_kernel_scheduler(t_cpu* cpu)
{
  int codigo_operacion = recibir_operacion(cpu->socket_kernel_scheduler);
  uint32_t pid;
  if (codigo_operacion == OP_CONTINUAR_PROCESO)
  {
    int size;
    void* buffer = recibir_buffer(&size, cpu->socket_kernel_scheduler);
    pid = *(uint32_t*)buffer;
    free(buffer);

    log_info(cpu->logger,
             "## PID recibido: %u - Iniciando ciclo de instrucción", pid);
  }
  else
  {
    cerrar_modulo(cpu);
  }
  return pid;
}

bool pedir_contexto_kernel_memory(t_cpu* cpu, uint32_t pid)
{
  return enviar_buffer(OP_PEDIR_CONTEXTO, &pid, sizeof(uint32_t),
                       cpu->socket_kernel_memory);
}

t_contexto* recibir_contexto_kernel_memory(t_cpu* cpu)
{
  escuchar_kernel_memory(cpu);

  int size;
  void* buffer = recibir_buffer(&size, cpu->socket_kernel_memory);
  t_contexto* contexto = malloc(sizeof(t_contexto));
  memcpy(contexto, buffer, size);
  free(buffer);
  return contexto;
}

void ejecutar_ciclo_instruccion(t_cpu* cpu, uint32_t pid, t_contexto* contexto)
{
  bool seguir = true;
  while (seguir)
  {
    char* instruccion_KM = etapa_fetch(cpu, pid, contexto->PC);
    log_info(cpu->logger, "## PID: %u - FETCH - Program Counter: %u", pid,
             contexto->PC);

    t_instruccion* instruccion = etapa_decode(instruccion_KM);

    uint32_t pc_inicial = contexto->PC;
    log_info(cpu->logger, "## PID: %u - Ejecutando: %s ", pid, instruccion_KM);
    free(instruccion_KM);

    seguir = etapa_execute(cpu, contexto, instruccion, pid);

    if (seguir && pc_inicial == contexto->PC)
      contexto->PC++;

    if (seguir)
      if (!enviar_string(OP_CICLO_CPU_OK, "OK", cpu->socket_kernel_scheduler))
      {
        log_error(cpu->logger, "## Error en la confirmación del fin de ciclo");
        cerrar_modulo(cpu);
        return;
      }
    log_info(cpu->logger, "Scheduler notificado del fin de ciclo");
    seguir = check_interrupt(cpu, pid, contexto);

    destruir_instruccion(instruccion);
  }
  enviar_contexto_actualizado(cpu, pid, contexto);
}

char* etapa_fetch(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  pedir_instruccion_kernel_memory(cpu, pid, pc);

  return recibir_instruccion_kernel_memory(cpu);
}

void pedir_instruccion_kernel_memory(t_cpu* cpu, uint32_t pid, uint32_t pc)
{
  t_paquete* paquete = crear_paquete(OP_SIGUIENTE_INSTRUCCION);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, &pc, sizeof(uint32_t));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## error en la petición de la instrucción");
    cerrar_modulo(cpu);
  }
  log_info(cpu->logger, "Instrucción pedida correctamente");
  eliminar_paquete(paquete);
}

char* recibir_instruccion_kernel_memory(t_cpu* cpu)
{
  escuchar_kernel_memory(cpu);

  return recibir_string(cpu->socket_kernel_memory);
}

t_instruccion* etapa_decode(char* instruccion_KM)
{
  t_instruccion* instrucion = malloc(sizeof(t_instruccion));
  instrucion->cantidad_parametros = 0;

  char** partes = string_split(instruccion_KM, " ");  // array de strings

  instrucion->nombre = strdup(partes[0]);

  for (int i = 1; partes[i] != NULL; i++)
  {
    instrucion->parametros[i - 1] = strdup(partes[i]);
    instrucion->cantidad_parametros++;
  }

  // liberar el array temporal
  for (int i = 0; partes[i] != NULL; i++)
    free(partes[i]);
  free(partes);

  return instrucion;
}

bool etapa_execute(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid)
{
  t_handler handler = dictionary_get(cpu->handlers, instruccion->nombre);

  if (handler == NULL)
  {
    log_error(cpu->logger, "## instruccion desconocida: %s",
              instruccion->nombre);
    cerrar_modulo(cpu);
  }

  return handler(cpu, contexto, instruccion, pid);
}

bool check_interrupt(t_cpu* cpu, uint32_t pid, t_contexto* contexto)
{
  int codigo = recibir_operacion(cpu->socket_kernel_scheduler);

  if (codigo == OP_INTERRUPCION)
  {
    log_info(cpu->logger, "## Interrupción recibida");
    return false;
  }
  else if (codigo == OP_SIN_INTERRUPCION)
  {
    return true;
  }
  log_error(cpu->logger, "## Operacion no reconocida: %d", codigo);
  cerrar_modulo(cpu);
  return 0;
}

void enviar_contexto_actualizado(t_cpu* cpu, uint32_t pid,
                                 t_contexto* contexto_actualizado)
{
  t_paquete* paquete = crear_paquete(OP_CONTEXTO_ACTUALIZADO);
  agregar_a_paquete(paquete, &pid, sizeof(uint32_t));
  agregar_a_paquete(paquete, contexto_actualizado, sizeof(t_contexto));
  if (!enviar_paquete(paquete, cpu->socket_kernel_memory))
  {
    log_error(cpu->logger, "## error en el envio del contexto actualizado");
    cerrar_modulo(cpu);
  }
  log_info(cpu->logger, "Envio correcto del contexto actualizado");
  eliminar_paquete(paquete);
}

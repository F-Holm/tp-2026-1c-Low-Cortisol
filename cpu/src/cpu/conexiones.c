#include "cpu/conexiones.h"

#include <commons/log.h>
#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/kernel_memory_cpu.h"

bool iniciar_conexion_scheduler(t_cpu* cpu)
{
  // CONEXION CON EL KERNEL SCHEDULER
  char* ip_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_IP");

  char* puerto_kernel_scheduler =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_PUERTO");

  cpu->socket_kernel_scheduler =
      crear_conexion(ip_kernel_scheduler, puerto_kernel_scheduler);

  // Handshake con Kernel scheduler
  if (enviar_handshake(MID_CPU, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger,
             "handshake correctamente enviado al kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo en el envio del hanshake con kernel scheduler");
  }

  int id_modulo = recibir_handshake(cpu->socket_kernel_scheduler);
  if (id_modulo != MID_KERNEL_SCHEDULER)
  {
    log_error(cpu->logger,
              "## Error al recibir el Handshake con Kernel_scheduler");
    close(cpu->socket_kernel_scheduler);
    return false;
  }
  log_info(cpu->logger, "Handshake recibido con exitoso del Kernel scheduler");

  return true;
}

bool iniciar_conexion_kmemory(t_cpu* cpu)
{
  // CONEXION CON EL KERNEL MEMORY
  char* ip_kernel_memory =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_IP");

  char* puerto_kernel_memory =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_PUERTO");

  cpu->socket_kernel_memory =
      crear_conexion(ip_kernel_memory, puerto_kernel_memory);

  // Handshake con Kernel Memory
  if (enviar_handshake(MID_CPU, cpu->socket_kernel_memory))
  {
    log_info(cpu->logger, "handshake correctamente enviado al kernel memory");
  }
  else
  {
    log_error(cpu->logger,
              "## fallo en el envio del hanshake con kernel memory");
  }

  int id_modulo = recibir_handshake(cpu->socket_kernel_memory);
  if (id_modulo != MID_KERNEL_MEMORY)
  {
    log_error(cpu->logger,
              "## Error al recibir el Handshake con Kernel_Memory");
    close(cpu->socket_kernel_memory);
    return false;
  }
  log_info(cpu->logger, "Handshake recibido del Kernel Memory");

  return true;
}

bool conectar_memory_stick(t_cpu* cpu)
{
  t_list* lista_paquete;
  lista_paquete = recibir_paquete(cpu->socket_kernel_memory);

  char ip_stick[16];
  char puerto_stick[6];
  int nuevo_socket;
  uint32_t tamanio_recibido;

  if (!manejar_paquete(cpu, lista_paquete, ip_stick, puerto_stick, &tamanio_recibido))
    return false;

  nuevo_socket = crear_conexion(ip_stick, puerto_stick);

  if (nuevo_socket <= 0)
  {
    log_error(cpu->logger, "## Error en la conexión con Memory stick");
    return false;
  }

  log_info(cpu->logger, "## Conectandose a memory stick con ip %s y puerto %s",
           ip_stick, puerto_stick);

  if (!handshake_memory_stick(cpu, nuevo_socket))
  {
    log_error(cpu->logger, "## Error en el handshake con Memory stick");
    return false;
  }

  if (!enviar_string(OP_ID_CPU, cpu->id, nuevo_socket))
  {
    log_error(cpu->logger, "## Error en el envío de ID con Memory stick");
    close(nuevo_socket);
    return false;
  }

  t_memory_stick_info* nuevo_stick = malloc(sizeof(t_memory_stick_info));
  nuevo_stick->socket_MS  = nuevo_socket;
  nuevo_stick->tamanio = tamanio_recibido;
  nuevo_stick->offset  = calcular_offset(cpu->memory_sticks); 

  list_add(cpu->memory_sticks, nuevo_stick);

  return true;
}

bool handshake_memory_stick(t_cpu* cpu, int nuevo_socket)
{
  // Handshake con memory stick
  if (!enviar_handshake(MID_CPU, nuevo_socket))
  {
    close(nuevo_socket);
    return false;
  }

  int id_modulo = recibir_handshake(nuevo_socket);
  if (id_modulo != MID_MEMORY_STICK)
  {
    close(nuevo_socket);
    return false;
  }
  log_info(cpu->logger, "## Handshake exitoso con Memory stick");

  return true;
}
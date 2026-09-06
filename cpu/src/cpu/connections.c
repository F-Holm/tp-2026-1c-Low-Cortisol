#include "cpu/connections.h"

#include <stdio.h>

#include "cpu/cleanup.h"
#include "cpu/cpu.h"
#include "utils/log.h"

bool connect_to_kernel_scheduler(t_cpu* cpu)
{
  char* kernel_scheduler_ip =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_IP");

  char* kernel_scheduler_port =
      config_get_string_value(cpu->config, "KERNEL_SCHEDULER_PORT");

  cpu->socket_kernel_scheduler =
      crear_conexion(kernel_scheduler_ip, kernel_scheduler_port);

  if (enviar_handshake(MID_CPU, cpu->socket_kernel_scheduler))
  {
    log_info(cpu->logger, "handshake sent to the kernel scheduler");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the handshake to the kernel scheduler");
  }

  int module_id = recibir_handshake(cpu->socket_kernel_scheduler);
  if (module_id != MID_KERNEL_SCHEDULER)
  {
    log_error(cpu->logger,
              "## Error receiving the handshake from the kernel scheduler");
    close(cpu->socket_kernel_scheduler);
    return false;
  }
  log_info(cpu->logger, "Handshake received from the kernel scheduler");

  return true;
}

bool connect_to_kernel_memory(t_cpu* cpu)
{
  char* kernel_memory_ip =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_IP");

  char* kernel_memory_port =
      config_get_string_value(cpu->config, "KERNEL_MEMORY_PORT");

  cpu->socket_kernel_memory =
      crear_conexion(kernel_memory_ip, kernel_memory_port);

  if (enviar_handshake(MID_CPU, cpu->socket_kernel_memory))
  {
    log_info(cpu->logger, "handshake sent to the kernel memory");
  }
  else
  {
    log_error(cpu->logger,
              "## failed to send the handshake to the kernel memory");
  }

  int module_id = recibir_handshake(cpu->socket_kernel_memory);
  if (module_id != MID_KERNEL_MEMORY)
  {
    log_error(cpu->logger,
              "## Error receiving the handshake from the kernel memory");
    close(cpu->socket_kernel_memory);
    return false;
  }
  log_info(cpu->logger, "Handshake received from the kernel memory");

  return true;
}

bool connect_memory_stick(t_cpu* cpu)
{
  t_list* packet;
  packet = recibir_paquete(cpu->socket_kernel_memory);

  char stick_ip[16];
  char stick_port[6];
  int new_socket;
  uint32_t received_size;

  if (!parse_stick_packet(cpu, packet, stick_ip, stick_port, &received_size))
    return false;

  new_socket = crear_conexion(stick_ip, stick_port);

  if (new_socket <= 0)
  {
    log_error(cpu->logger, "## Error connecting to the memory stick");
    return false;
  }

  log_info(cpu->logger, "Connecting to memory stick at ip %s and port %s",
           stick_ip, stick_port);

  if (!handshake_memory_stick(cpu, new_socket))
  {
    log_error(cpu->logger, "## Error in the handshake with the memory stick");
    return false;
  }

  if (!enviar_string(OP_ID_CPU, cpu->id, new_socket))
  {
    log_error(cpu->logger, "## Error sending the ID to the memory stick");
    close(new_socket);
    return false;
  }

  t_memory_stick_info* new_stick = malloc(sizeof(t_memory_stick_info));
  new_stick->socket_ms = new_socket;
  new_stick->size = received_size;
  new_stick->offset = compute_offset(cpu->memory_sticks);

  list_add(cpu->memory_sticks, new_stick);

  return true;
}

uint32_t compute_offset(t_list* sticks)
{
  uint32_t offset = 0;
  for (int i = 0; i < list_size(sticks); i++)
  {
    t_memory_stick_info* stick = list_get(sticks, i);
    offset += stick->size;
  }
  return offset;
}

bool handshake_memory_stick(t_cpu* cpu, int new_socket)
{
  if (!enviar_handshake(MID_CPU, new_socket))
  {
    close(new_socket);
    return false;
  }

  int module_id = recibir_handshake(new_socket);
  if (module_id != MID_MEMORY_STICK)
  {
    close(new_socket);
    return false;
  }
  log_info(cpu->logger, "Handshake successful with the memory stick");

  return true;
}

void notify_bsod(t_cpu* cpu)
{
  if (!enviar_string(OP_STICK_DESCONECTADO, "MS disconnected",
                     cpu->socket_kernel_memory))
    log_info(cpu->logger, "Kernel Memory disconnected");
  else
    log_info(cpu->logger, "Kernel Memory notified of BSOD");
}

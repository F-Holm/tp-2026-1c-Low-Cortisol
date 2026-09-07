#include "memory_stick/kernel_memory.h"

#include <stdio.h>

#include "utils/msg.h"

bool handshake_km(int socket_km, t_log* logger)
{
  if (!send_handshake(MID_MEMORY_STICK, socket_km))
  {
    log_error(logger, "## Could not send the handshake to Kernel Memory");
    return false;
  }
  if (receive_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    log_error(logger, "## Could not receive the handshake from Kernel Memory");
    return false;
  }
  log_debug(logger, "Handshake successful with Kernel Memory");
  return true;
}

bool send_size(int socket_km, char* size, t_log* logger)
{
  if (!send_string(OP_MEMORY_SIZE, size, socket_km))
  {
    log_error(logger, "## Could not send the size to Kernel Memory");
    return false;
  }
  log_debug(logger, "Size reported to Kernel Memory");
  return true;
}

int connect_to_kernel_memory(char* ip, char* port, char* size, t_log* logger)
{
  int socket_km = connect_km(ip, port, logger);
  if (socket_km <= 0)
    return -1;

  if (!handshake_km(socket_km, logger))
    return -1;

  if (!send_size(socket_km, size, logger))
    return -1;

  return socket_km;
}

int connect_km(char* ip, char* port, t_log* logger)
{
  int socket_km = create_connection(ip, port);
  if (socket_km <= 0)
  {
    log_error(logger, "## Connection error with Kernel Memory");
    return -1;
  }
  log_info(logger, "## Connected to Kernel Memory");
  return socket_km;
}

bool send_cpu_server_port_to_km(int socket, uint16_t port)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%u", port);
  return send_string(OP_PORT, buffer, socket);
}

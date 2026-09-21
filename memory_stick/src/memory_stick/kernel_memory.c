#include "memory_stick/kernel_memory.h"

#include <stdio.h>

#include "utils/msg.h"

bool handshake_km(t_socket* socket_km, t_log* logger)
{
  if (!send_handshake(MID_MEMORY_STICK, socket_km))
  {
    log_error(logger, "Could not send the handshake to Kernel Memory");
    return false;
  }
  if (receive_handshake(socket_km) != MID_KERNEL_MEMORY)
  {
    log_error(logger, "Could not receive the handshake from Kernel Memory");
    return false;
  }
  log_debug(logger, "Handshake successful with Kernel Memory");
  return true;
}

bool send_size(t_socket* socket_km, char* size, t_log* logger)
{
  if (!send_string(OP_MEMORY_SIZE, size, socket_km))
  {
    log_error(logger, "Could not send the size to Kernel Memory");
    return false;
  }
  log_debug(logger, "Size reported to Kernel Memory");
  return true;
}

t_socket* connect_to_kernel_memory(char* ip, char* port, char* size,
                                   t_log* logger)
{
  t_socket* socket_km = connect_km(ip, port, logger);
  if (socket_km == NULL)
    return NULL;

  if (!handshake_km(socket_km, logger) || !send_size(socket_km, size, logger))
  {
    socket_destroy(socket_km);
    return NULL;
  }

  return socket_km;
}

t_socket* connect_km(char* ip, char* port, t_log* logger)
{
  t_socket* socket_km = socket_create(SOCKET_KIND_CLIENT, ip, port, false);
  if (socket_km == NULL)
  {
    log_error(logger, "Connection error with Kernel Memory");
    return NULL;
  }
  log_info(logger, "Connected to Kernel Memory");
  return socket_km;
}

bool send_cpu_server_port_to_km(t_socket* socket, uint16_t port)
{
  char buffer[6];
  snprintf(buffer, sizeof(buffer), "%u", port);
  return send_string(OP_PORT, buffer, socket);
}

#include "memory_stick/cpu.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "utils/server.h"

int create_server_cpu(void)
{
  return iniciar_servidor(NULL);
}

uint16_t get_puerto_cpu(int socket_server_cpu)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(socket_server_cpu, (struct sockaddr*)&addr, &len);
  return ntohs(addr.sin_port);
}

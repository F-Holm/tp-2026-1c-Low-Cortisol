#include "memory_stick/cpu.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

int create_server_cpu();

uint16_t get_puerto_cpu(int socket)
{
  struct sockaddr_in addr;
  socklen_t len = sizeof(addr);
  getsockname(socket, (struct sockaddr*)&addr, &len);
  return ntohs(addr.sin_port);
}

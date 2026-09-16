#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/log.h"
#include "utils/msg.h"

t_log* km_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "KM-test", false, LOG_LEVEL_ERROR, true);
  cr_assert_not_null(logger);
  return logger;
}

t_hole* km_make_hole(int base, int size)
{
  t_hole* hole = malloc(sizeof(t_hole));
  hole->base = base;
  hole->size = size;
  return hole;
}

t_segment* km_make_segment(uint32_t id, uint32_t pid, int base, int size)
{
  t_segment* segment = calloc(1, sizeof(t_segment));
  segment->id = id;
  segment->pid = pid;
  segment->base = base;
  segment->size = size;
  return segment;
}

t_stick_data* km_make_stick(int size)
{
  t_stick_data* stick = calloc(1, sizeof(t_stick_data));
  stick->stick_size = size;
  return stick;
}

int km_listen_ephemeral(char* port_out, int port_len)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  snprintf(port_out, port_len, "%d", ntohs(address.sin_port));

  return listen_fd;
}

int km_connected_pair(int* server_out)
{
  char port[16];
  int listen_fd = km_listen_ephemeral(port, sizeof(port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

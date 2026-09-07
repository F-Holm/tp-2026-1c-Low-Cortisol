#include "support.h"

#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/collections/list.h"
#include "utils/msg.h"

t_log* cpu_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "CPU-test", false, LOG_LEVEL_ERROR, false);
  cr_assert_not_null(logger);
  return logger;
}

int cpu_connected_pair(int* server_out)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);
  char port[16];
  snprintf(port, sizeof(port), "%d", ntohs(address.sin_port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

t_context* cpu_make_context(void)
{
  t_context* context = malloc(sizeof(t_context));
  context->registers = calloc(1, sizeof(t_registers));
  context->segment_table = list_create();
  context->segment_changed = false;
  return context;
}

void cpu_destroy_context(t_context* context)
{
  list_destroy_and_destroy_elements(context->segment_table, free);
  free(context->registers);
  free(context);
}

t_segment* cpu_make_segment(uint32_t id, int base, int size)
{
  t_segment* segment = calloc(1, sizeof(t_segment));
  segment->id = id;
  segment->base = base;
  segment->size = size;
  return segment;
}

t_memory_stick_info* cpu_make_stick(uint32_t offset, uint32_t size)
{
  t_memory_stick_info* stick = calloc(1, sizeof(t_memory_stick_info));
  stick->offset = offset;
  stick->size = size;
  return stick;
}

#include "support.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cpu/cpu.h"
#include "cpu/registers.h"
#include "utils/collections/list.h"
#include "utils/log.h"
#include "utils/registers_cpu.h"
#include "utils/sockets.h"

t_log* cpu_quiet_logger(void)
{
  t_log* logger = log_create(NULL, "CPU-test", false, LOG_LEVEL_ERROR, false);
  cr_assert_not_null(logger);
  return logger;
}

t_socket* cpu_listen_ephemeral(char* port_out, int port_len)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  snprintf(port_out, port_len, "%d", socket_get_local_port(listener));

  return listener;
}

t_socket* cpu_connected_pair(t_socket** server_out)
{
  char port[16];
  t_socket* listener = cpu_listen_ephemeral(port, sizeof(port));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

char* cpu_write_temp_config(const char* contents)
{
  char path[] = "/tmp/cpu_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1, "could not create a temp config file");

  FILE* file = fdopen(fd, "w");
  fputs(contents, file);
  fclose(file);

  return strdup(path);
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

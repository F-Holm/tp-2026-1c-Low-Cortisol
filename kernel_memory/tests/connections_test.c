#include "kernel_memory/connections.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

Test(km_connections, compute_total_memory_sums_the_stick_sizes)
{
  t_list* sticks = list_create();
  list_add(sticks, km_make_stick(256));
  list_add(sticks, km_make_stick(512));
  cr_assert_eq(compute_total_memory(sticks, &mutex), 768);
  list_destroy_and_destroy_elements(sticks, free);
}

/* ── receive_cpu_id ────────────────────────────────────────────────────── */

Test(km_connections, receive_cpu_id_parses_the_announced_id)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "7", server_fd));

  t_log* logger = km_quiet_logger();
  t_cpu_data cpu_data = {.socket_cpu = client_fd, .logger = logger};

  cr_assert(receive_cpu_id(&cpu_data));
  cr_assert_eq(cpu_data.id, 7);

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_connections,
     receive_cpu_id_fails_and_closes_the_socket_on_a_wrong_op_code)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_SIZE, "7", server_fd));

  t_log* logger = km_quiet_logger();
  t_cpu_data cpu_data = {.socket_cpu = client_fd, .logger = logger};

  cr_assert_not(receive_cpu_id(&cpu_data));

  close(server_fd);
  log_destroy(logger);
}

/* ── receive_stick_size / receive_stick_listen_port ───────────────────── */

Test(km_connections, receive_stick_size_parses_the_announced_size)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_MEMORY_SIZE, "1024", server_fd));

  t_log* logger = km_quiet_logger();
  t_stick_data stick_data = {.socket_stick = client_fd, .logger = logger};

  cr_assert(receive_stick_size(&stick_data));
  cr_assert_eq(stick_data.stick_size, 1024);

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_connections, receive_stick_size_fails_on_a_wrong_op_code)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "1024", server_fd));

  t_log* logger = km_quiet_logger();
  t_stick_data stick_data = {.socket_stick = client_fd, .logger = logger};

  cr_assert_not(receive_stick_size(&stick_data));

  close(server_fd);
  log_destroy(logger);
}

Test(km_connections, receive_stick_listen_port_parses_the_announced_port)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_PORT, "9090", server_fd));

  t_log* logger = km_quiet_logger();
  t_stick_data stick_data = {.socket_stick = client_fd, .logger = logger};

  cr_assert(receive_stick_listen_port(&stick_data));
  cr_assert_eq(stick_data.stick_port, 9090);

  close(client_fd);
  close(server_fd);
  log_destroy(logger);
}

Test(km_connections, receive_stick_listen_port_fails_on_a_wrong_op_code)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "9090", server_fd));

  t_log* logger = km_quiet_logger();
  t_stick_data stick_data = {.socket_stick = client_fd, .logger = logger};

  cr_assert_not(receive_stick_listen_port(&stick_data));

  close(server_fd);
  log_destroy(logger);
}

/* ── add_stick_connection / add_cpu_connection ────────────────────────── */

Test(km_connections, add_stick_connection_appends_to_the_list)
{
  t_list* sticks = list_create();
  pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
  t_kernel_memory_data kernel_data = {.connected_sticks = sticks,
                                      .socket_list_mutex = &list_mutex};
  t_stick_data* stick = km_make_stick(128);

  add_stick_connection(&kernel_data, stick);

  cr_assert_eq(list_size(sticks), 1);
  cr_assert_eq(list_get(sticks, 0), stick);

  list_destroy_and_destroy_elements(sticks, free);
}

Test(km_connections, add_cpu_connection_appends_to_the_list)
{
  t_list* cpus = list_create();
  pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
  t_kernel_memory_data kernel_data = {.connected_cpus = cpus,
                                      .socket_list_mutex = &list_mutex};
  t_cpu_data cpu = {.id = 3};

  add_cpu_connection(&kernel_data, &cpu);

  cr_assert_eq(list_size(cpus), 1);
  cr_assert_eq(list_get(cpus, 0), &cpu);

  list_destroy(cpus);
}

/* ── send_connected_sticks / send_cpu_connection ──────────────────────── */

Test(km_connections, send_connected_sticks_tells_the_cpu_about_every_stick)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);

  t_list* sticks = list_create();
  t_stick_data* stick = km_make_stick(2048);
  stick->stick_port = 5000;
  strcpy(stick->ip_memory_stick, "127.0.0.1");
  list_add(sticks, stick);
  pthread_mutex_t list_mutex = PTHREAD_MUTEX_INITIALIZER;
  t_cpu_data cpu_data = {.socket_cpu = client_fd};

  send_connected_sticks(sticks, &list_mutex, &cpu_data);

  cr_assert_eq(receive_op_code(server_fd), OP_PACKET);
  t_list* fields = receive_packet(server_fd);
  cr_assert_eq(list_size(fields), 3);
  cr_assert_str_eq((char*)list_get(fields, 0), "127.0.0.1");
  cr_assert_str_eq((char*)list_get(fields, 1), "5000");
  cr_assert_eq(*(int*)list_get(fields, 2), 2048);

  list_destroy_and_destroy_elements(fields, free);
  list_destroy_and_destroy_elements(sticks, free);
  close(client_fd);
  close(server_fd);
}

Test(km_connections, send_cpu_connection_tells_every_cpu_about_the_new_stick)
{
  int server_fd;
  int client_fd = km_connected_pair(&server_fd);

  t_list* cpus = list_create();
  t_cpu_data cpu_data = {.socket_cpu = client_fd};
  list_add(cpus, &cpu_data);
  t_stick_data* stick = km_make_stick(4096);
  stick->stick_port = 6000;
  strcpy(stick->ip_memory_stick, "127.0.0.1");

  send_cpu_connection(stick, cpus);

  cr_assert_eq(receive_op_code(server_fd), OP_PACKET);
  t_list* fields = receive_packet(server_fd);
  cr_assert_eq(list_size(fields), 3);
  cr_assert_str_eq((char*)list_get(fields, 0), "127.0.0.1");
  cr_assert_str_eq((char*)list_get(fields, 1), "6000");
  cr_assert_eq(*(int*)list_get(fields, 2), 4096);

  list_destroy_and_destroy_elements(fields, free);
  list_destroy(cpus);
  free(stick);
  close(client_fd);
  close(server_fd);
}

Test(km_connections, send_cpu_connection_is_a_no_op_with_no_cpus_connected)
{
  t_list* cpus = list_create();
  t_stick_data* stick = km_make_stick(4096);

  send_cpu_connection(stick, cpus);

  list_destroy(cpus);
  free(stick);
}

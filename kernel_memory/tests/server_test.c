#include "kernel_memory/server.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "support.h"
#include "utils/msg.h"
#include "utils/swap_km.h"

static t_kernel_memory_data* km_stub_kernel_data(t_log* logger)
{
  return init_kernel_memory_data(NULL, NULL, 0, 0, 1024, AS_WORST, logger);
}

Test(km_server, handshake_rejects_an_unrecognized_identifier)
{
  t_socket* server_fd;
  t_socket* client_fd = km_connected_pair(&server_fd);
  cr_assert(send_handshake(MID_IO, client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert_not(handshake(kernel_data, server_fd));

  socket_destroy(client_fd);
  socket_destroy(server_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, handshake_registers_the_kernel_scheduler)
{
  t_socket* server_fd;
  t_socket* client_fd = km_connected_pair(&server_fd);
  cr_assert(send_handshake(MID_KERNEL_SCHEDULER, client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert(handshake(kernel_data, server_fd));
  cr_assert_eq(receive_handshake(client_fd), MID_KERNEL_MEMORY);
  cr_assert_eq(atomic_load(&(kernel_data->socket_scheduler)), server_fd);

  /* Closing our end of the peer socket makes the detached scheduler-listener
   * thread's next read fail, so it shuts itself down and decrements
   * active_threads; free_kernel_memory_data then waits for exactly that. */
  socket_destroy(client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, handshake_rejects_a_cpu_before_the_scheduler_is_connected)
{
  t_socket* server_fd;
  t_socket* client_fd = km_connected_pair(&server_fd);
  cr_assert(send_handshake(MID_CPU, client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert(handshake(kernel_data, server_fd));
  cr_assert_eq(list_size(kernel_data->connected_cpus), 0);

  socket_destroy(client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, handshake_registers_a_cpu_once_the_scheduler_is_connected)
{
  t_socket* scheduler_server_fd;
  t_socket* scheduler_client_fd = km_connected_pair(&scheduler_server_fd);
  cr_assert(send_handshake(MID_KERNEL_SCHEDULER, scheduler_client_fd));

  t_socket* cpu_server_fd;
  t_socket* cpu_client_fd = km_connected_pair(&cpu_server_fd);
  cr_assert(send_handshake(MID_CPU, cpu_client_fd));
  cr_assert(send_string(OP_ID_CPU, "9", cpu_client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert(handshake(kernel_data, scheduler_server_fd));
  cr_assert_eq(receive_handshake(scheduler_client_fd), MID_KERNEL_MEMORY);

  cr_assert(handshake(kernel_data, cpu_server_fd));
  cr_assert_eq(receive_handshake(cpu_client_fd), MID_KERNEL_MEMORY);
  cr_assert_eq(receive_op_code(cpu_client_fd), OP_MAX_SEGMENT_SIZE);
  cr_assert_eq(list_size(kernel_data->connected_cpus), 1);
  t_cpu_data* cpu_data = list_get(kernel_data->connected_cpus, 0);
  cr_assert_eq(cpu_data->id, 9);

  socket_destroy(scheduler_client_fd);
  socket_destroy(cpu_client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, handshake_registers_a_memory_stick)
{
  t_socket* server_fd;
  t_socket* client_fd = km_connected_pair(&server_fd);
  cr_assert(send_handshake(MID_MEMORY_STICK, client_fd));
  cr_assert(send_string(OP_MEMORY_SIZE, "2048", client_fd));
  cr_assert(send_string(OP_PORT, "9091", client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert(handshake(kernel_data, server_fd));
  cr_assert_eq(receive_handshake(client_fd), MID_KERNEL_MEMORY);
  cr_assert_eq(list_size(kernel_data->connected_sticks), 1);
  t_stick_data* stick = list_get(kernel_data->connected_sticks, 0);
  cr_assert_eq(stick->stick_size, 2048);
  cr_assert_eq(stick->stick_port, 9091);
  cr_assert_str_eq(stick->ip_memory_stick, "127.0.0.1");
  cr_assert_eq(kernel_data->main_memory->total_size, 2048);

  socket_destroy(client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, handshake_registers_swap)
{
  t_socket* server_fd;
  t_socket* client_fd = km_connected_pair(&server_fd);
  cr_assert(send_handshake(MID_SWAP, client_fd));
  t_swap_config config = {.swap_size = 4096, .block_size = 64};
  cr_assert(send_buffer(OP_INFO_SWAP, &config, sizeof(config), client_fd));

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data = km_stub_kernel_data(logger);

  cr_assert(handshake(kernel_data, server_fd));
  cr_assert_eq(receive_handshake(client_fd), MID_KERNEL_MEMORY);
  t_swap_data* swap_data = atomic_load(&kernel_data->swap_data);
  cr_assert_not_null(swap_data);
  cr_assert_eq(swap_data->swap_size, 4096);
  cr_assert_eq(swap_data->block_size, 64);

  socket_destroy(client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

Test(km_server, accept_client_accepts_and_handshakes_one_connection)
{
  char port[16];
  t_socket* listen_fd = km_listen_ephemeral(port, sizeof(port));
  t_socket* client_fd =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_neq(client_fd, -1);
  cr_assert(send_handshake(MID_IO, client_fd)); /* unrecognized -> false */

  t_log* logger = km_quiet_logger();
  t_kernel_memory_data* kernel_data =
      init_kernel_memory_data(listen_fd, NULL, 0, 0, 1024, AS_WORST, logger);

  cr_assert_not(accept_client(kernel_data));

  socket_destroy(client_fd);
  free_kernel_memory_data(kernel_data);
  log_destroy(logger);
}

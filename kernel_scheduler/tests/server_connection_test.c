#include <criterion/criterion.h>
#include <unistd.h>

#include "kernel_scheduler/connections/server.h"
#include "support.h"
#include "utils/msg.h"
#include "utils/threads.h"

static void* server_listen_thread(void* arg)
{
  server_listen((t_listen_server_data*)arg);
  return NULL;
}

Test(ks_server_connection, creates_a_listening_socket_on_an_ephemeral_port)
{
  t_log* logger = ks_quiet_logger();

  t_socket* socket_server = create_socket_server("0", logger);
  cr_assert_geq(socket_server, 0);

  socket_destroy(socket_server);
  log_destroy(logger);
}

Test(ks_server_connection,
     server_listen_registers_io_rejects_garbage_and_shuts_down_cleanly)
{
  char port[16];
  t_socket* socket_server = ks_listen_ephemeral(port, sizeof(port));

  t_socket* km_server_fd;
  t_socket* km_client_fd = ks_connected_pair(&km_server_fd);
  cr_assert(send_string(OP_PROCESS_STARTED, "started", km_server_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  queues->km_socket = km_client_fd;
  t_mutex_list* mutex_list = init_list_mutex();
  t_socket* km_socket = queues->km_socket;

  t_listen_server_data data;
  init_data_server_listen(&data, socket_server, logger, mutex_list, queues,
                          km_socket, "initial.txt");

  thrd_t listener;
  thrd_create(&listener, server_listen_thread, &data);

  /* A valid IO registration: the server should answer with its own
   * handshake and keep the connection open. */
  t_socket* io_fd = socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(io_fd);
  cr_assert(send_handshake(MID_IO, io_fd));
  cr_assert(send_string(OP_IO_TYPE, "STDIN", io_fd));
  cr_assert_eq(receive_handshake(io_fd), MID_KERNEL_SCHEDULER);

  /* An unrecognized handshake: the server should close the connection
   * without replying. */
  t_socket* garbage_fd =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(garbage_fd);
  cr_assert(send_handshake(MID_SWAP, garbage_fd));
  cr_assert_eq(receive_op_code(garbage_fd), OP_CODE_ERROR);

  /* Shutting the listening socket down makes accept() return an error,
   * ending server_listen's loop; it then tears io/cpu state down itself. */
  socket_shutdown(socket_server, SOCKET_SHUTDOWN_BOTH);
  thrd_join(listener, NULL);

  socket_destroy(io_fd);
  socket_destroy(garbage_fd);
  socket_destroy(socket_server);
  socket_destroy(km_server_fd);
  destroy_list_mutex(mutex_list);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

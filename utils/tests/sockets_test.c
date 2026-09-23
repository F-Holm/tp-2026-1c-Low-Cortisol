#include "utils/sockets.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdio.h>
#include <threads.h>

TestSuite(sockets, .timeout = 5.0);

static t_socket* connected_pair(t_socket** server_out, bool with_mutex)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, with_mutex);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, with_mutex);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

Test(sockets, create_and_accept_establish_a_loopback_connection)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  cr_assert_gt(socket_get_local_port(server), 0);

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, create_client_to_an_unbound_port_fails)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));
  socket_destroy(listener);

  cr_assert_null(socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false));
}

Test(sockets, create_client_with_an_invalid_service_fails)
{
  cr_assert_null(
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", "not-a-port", false));
}

Test(sockets, send_and_receive_roundtrip_a_buffer)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  char message[] = "hello sockets";
  cr_assert(socket_send(client, message, sizeof(message)));

  char received[sizeof(message)];
  cr_assert(socket_receive(server, received, sizeof(received)));
  cr_assert_str_eq(received, message);

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, send_and_receive_on_a_null_socket_fail_without_crashing)
{
  char buffer[4] = {0};
  cr_assert_not(socket_send(NULL, buffer, sizeof(buffer)));
  cr_assert_not(socket_receive(NULL, buffer, sizeof(buffer)));
}

Test(sockets, mutex_lock_and_unlock_are_noops_without_with_mutex)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  cr_assert_not(client->has_mutex);
  socket_mutex_lock(client);    // no-op, must not crash
  socket_mutex_unlock(client);  // no-op, must not crash

  socket_destroy(client);
  socket_destroy(server);
}

typedef struct
{
  t_socket* socket;
  int* counter;
  int increments;
} t_lock_race_args;

static int increment_under_lock(void* raw_args)
{
  t_lock_race_args* args = raw_args;
  for (int i = 0; i < args->increments; i++)
  {
    socket_mutex_lock(args->socket);
    (*args->counter)++;
    socket_mutex_unlock(args->socket);
  }
  return 0;
}

Test(sockets, socket_mutex_lock_serializes_concurrent_access)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  cr_assert(client->has_mutex);

  int counter = 0;
  int increments = 1000;
  t_lock_race_args args1 = {client, &counter, increments};
  t_lock_race_args args2 = {client, &counter, increments};

  thrd_t thread1, thread2;
  thrd_create(&thread1, increment_under_lock, &args1);
  thrd_create(&thread2, increment_under_lock, &args2);
  thrd_join(thread1, NULL);
  thrd_join(thread2, NULL);

  cr_assert_eq(counter, 2 * increments);

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, closed_socket_rejects_further_sends)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  socket_close(client);
  cr_assert_not(socket_send(client, "x", 1));

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, destroy_and_close_on_null_do_not_crash)
{
  socket_destroy(NULL);
  socket_close(NULL);
}

Test(sockets, shutdown_both_prevents_further_sends_from_that_side)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  socket_shutdown(client, SOCKET_SHUTDOWN_BOTH);
  cr_assert_not(socket_send(client, "x", 1));

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, get_peer_ip_returns_the_address_of_the_connected_peer)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  char ip[16];
  cr_assert(socket_get_peer_ip(server, ip, sizeof(ip)));
  cr_assert_str_eq(ip, "127.0.0.1");
  cr_assert(socket_get_peer_ip(client, ip, sizeof(ip)));
  cr_assert_str_eq(ip, "127.0.0.1");

  socket_destroy(client);
  socket_destroy(server);
}

Test(sockets, get_peer_ip_fails_on_a_null_or_too_small_buffer_or_closed_socket)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, false);

  char ip[16];
  char tiny[4];
  cr_assert_not(socket_get_peer_ip(NULL, ip, sizeof(ip)));
  cr_assert_not(socket_get_peer_ip(client, tiny, sizeof(tiny)));
  socket_close(client);
  cr_assert_not(socket_get_peer_ip(client, ip, sizeof(ip)));

  socket_destroy(client);
  socket_destroy(server);
}

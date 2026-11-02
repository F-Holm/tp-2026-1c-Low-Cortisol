#include "utils/watchdog.h"

#include <criterion/criterion.h>
#include <poll.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "utils/collections/list.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/time.h"

/* Every test that touches a socket or a background thread gets a hard
 * ceiling so a protocol/synchronization bug shows up as a failure instead of
 * a hang. */
TestSuite(watchdog, .timeout = 5.0);

/* Opens a loopback connection on an ephemeral port. Returns the client
 * socket and, through @p server_out, the accepted server socket. Both must
 * be destroyed by the caller. */
static t_socket* connected_pair(t_socket** server_out, bool with_mutex)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, nullptr, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, with_mutex);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

/* Makes @p dead_end's next send fail: shutdown(SHUT_WR), not close() on
 * @p peer -- a single small send() on a loopback socket after the peer
 * merely closes often still succeeds silently (seen elsewhere in this
 * codebase), while shutting down our own write side is reliable. */
static void kill_connection(t_socket* dead_end, t_socket* peer)
{
  socket_shutdown(dead_end, SOCKET_SHUTDOWN_WRITE);
  socket_destroy(peer);
}

static void noop_on_disconnected(void* arg)
{
  (void)arg;
}

static void count_disconnection(void* arg)
{
  atomic_int* counter = arg;
  atomic_fetch_add(counter, 1);
}

/* Polls @p counter every 5ms until it reaches at least @p target or
 * @p timeout_ms elapses. */
static bool wait_until_at_least(atomic_int* counter, int target, int timeout_ms)
{
  int waited = 0;
  while (atomic_load(counter) < target && waited < timeout_ms)
  {
    time_sleep_ms(5);
    waited += 5;
  }
  return atomic_load(counter) >= target;
}

Test(watchdog, pings_every_socket_in_the_list)
{
  t_socket* server1;
  t_socket* client1 = connected_pair(&server1, true);
  t_socket* server2;
  t_socket* client2 = connected_pair(&server2, true);

  t_list* sockets = list_create();
  list_add(sockets, client1);
  list_add(sockets, client2);

  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 20,
                                        noop_on_disconnected, nullptr);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(server1), OP_CONNECTION_CHECK);
  free(receive_string(server1));
  cr_assert_eq(receive_op_code(server2), OP_CONNECTION_CHECK);
  free(receive_string(server2));

  watchdog_destroy(watchdog);
  list_destroy(sockets);
  socket_destroy(client1);
  socket_destroy(server1);
  socket_destroy(client2);
  socket_destroy(server2);
}

Test(watchdog, does_not_call_on_disconnected_while_every_socket_stays_alive)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);

  t_list* sockets = list_create();
  list_add(sockets, client);

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 10,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);

  /* Block for three full pings instead of sleeping a guessed amount of
   * time -- deterministically proves three cycles ran. */
  for (int i = 0; i < 3; i++)
  {
    cr_assert_eq(receive_op_code(server), OP_CONNECTION_CHECK);
    free(receive_string(server));
  }
  cr_assert_eq(atomic_load(&disconnections), 0);

  watchdog_destroy(watchdog);
  list_destroy(sockets);
  socket_destroy(client);
  socket_destroy(server);
}

Test(watchdog, calls_on_disconnected_when_a_ping_fails)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  kill_connection(client, server);

  t_list* sockets = list_create();
  list_add(sockets, client);

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 20,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);

  cr_assert(wait_until_at_least(&disconnections, 1, 2000));

  watchdog_destroy(watchdog);
  list_destroy(sockets);
  socket_destroy(client);
}

Test(watchdog, stops_calling_back_once_destroyed)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  kill_connection(client, server);

  t_list* sockets = list_create();
  list_add(sockets, client);

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 10,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);
  cr_assert(wait_until_at_least(&disconnections, 1, 2000));

  /* watchdog_destroy() joins the thread, so once it returns the counter can
   * never change again -- no race to guard against here. */
  watchdog_destroy(watchdog);
  int after_destroy = atomic_load(&disconnections);
  time_sleep_ms(100);  // far more than the 10ms cycle, were it still running
  cr_assert_eq(atomic_load(&disconnections), after_destroy);

  list_destroy(sockets);
  socket_destroy(client);
}

Test(watchdog, detects_the_dead_socket_when_mixed_in_with_alive_ones)
{
  t_socket* alive_server;
  t_socket* alive_client = connected_pair(&alive_server, true);
  t_socket* dead_server;
  t_socket* dead_client = connected_pair(&dead_server, true);
  kill_connection(dead_client, dead_server);

  t_list* sockets = list_create();
  list_add(sockets, alive_client);
  list_add(sockets, dead_client);

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 20,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);

  /* The dead socket gets flagged every pass -- combined with the zero seen
   * in does_not_call_on_disconnected_while_every_socket_stays_alive, this
   * shows the iterator keeps visiting every element instead of stopping at
   * (or skipping) the one that fails. */
  cr_assert(wait_until_at_least(&disconnections, 1, 2000));
  /* The alive socket in the same list, same passes, is still being pinged
   * normally. */
  cr_assert_eq(receive_op_code(alive_server), OP_CONNECTION_CHECK);
  free(receive_string(alive_server));

  watchdog_destroy(watchdog);
  list_destroy(sockets);
  socket_destroy(alive_client);
  socket_destroy(alive_server);
  socket_destroy(dead_client);
}

Test(watchdog, works_with_an_empty_socket_list)
{
  t_list* sockets = list_create();

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 10,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);

  time_sleep_ms(50);  // a handful of cycles over an empty list; must not crash

  watchdog_destroy(watchdog);
  cr_assert_eq(atomic_load(&disconnections), 0);
  list_destroy(sockets);
}

Test(watchdog, still_pings_when_given_a_sockets_list_mutex)
{
  mtx_t sockets_mutex;
  mtx_init(&sockets_mutex, mtx_plain);

  t_socket* server;
  t_socket* client = connected_pair(&server, true);

  t_list* sockets = list_create();
  list_add(sockets, client);

  t_watchdog* watchdog = watchdog_start(sockets, nullptr, &sockets_mutex, 20,
                                        noop_on_disconnected, nullptr);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(server), OP_CONNECTION_CHECK);
  free(receive_string(server));

  watchdog_destroy(watchdog);
  mtx_destroy(&sockets_mutex);
  list_destroy(sockets);
  socket_destroy(client);
  socket_destroy(server);
}

/* Whether @p socket has bytes waiting to be read, without blocking. */
static bool has_pending_data(t_socket* socket)
{
  struct pollfd pfd = {.fd = socket->handle, .events = POLLIN};
  return poll(&pfd, 1, 0) == 1 && (pfd.revents & POLLIN);
}

typedef struct
{
  int id;
  t_socket* socket;
} t_wrapped_socket;

static t_socket* unwrap_socket(void* element)
{
  return ((t_wrapped_socket*)element)->socket;
}

Test(watchdog, pings_through_get_socket_when_elements_wrap_a_socket)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  t_wrapped_socket wrapped = {.id = 1, .socket = client};

  t_list* elements = list_create();
  list_add(elements, &wrapped);

  t_watchdog* watchdog = watchdog_start(elements, unwrap_socket, nullptr, 10,
                                        noop_on_disconnected, nullptr);
  cr_assert_not_null(watchdog);

  cr_assert_eq(receive_op_code(server), OP_CONNECTION_CHECK);
  free(receive_string(server));

  watchdog_destroy(watchdog);
  list_destroy(elements);
  socket_destroy(client);
  socket_destroy(server);
}

Test(watchdog, reports_a_dead_wrapped_socket_through_get_socket)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  kill_connection(client, server);
  t_wrapped_socket wrapped = {.id = 1, .socket = client};

  t_list* elements = list_create();
  list_add(elements, &wrapped);

  atomic_int disconnections = 0;
  t_watchdog* watchdog = watchdog_start(elements, unwrap_socket, nullptr, 10,
                                        count_disconnection, &disconnections);
  cr_assert_not_null(watchdog);
  cr_assert(wait_until_at_least(&disconnections, 1, 2000));

  watchdog_destroy(watchdog);
  list_destroy(elements);
  socket_destroy(client);
}

typedef struct
{
  _Atomic(void*) received_arg;
  atomic_int calls;
} t_arg_probe;

static t_arg_probe probe;

static void record_callback_arg(void* arg)
{
  atomic_store(&probe.received_arg, arg);
  atomic_fetch_add(&probe.calls, 1);
}

Test(watchdog, passes_callback_arg_to_on_disconnected_as_is)
{
  t_socket* server;
  t_socket* client = connected_pair(&server, true);
  kill_connection(client, server);

  t_list* sockets = list_create();
  list_add(sockets, client);

  int marker;
  atomic_init(&probe.received_arg, nullptr);
  atomic_init(&probe.calls, 0);
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, nullptr, 10,
                                        record_callback_arg, &marker);
  cr_assert_not_null(watchdog);
  cr_assert(wait_until_at_least(&probe.calls, 1, 2000));
  watchdog_destroy(watchdog);

  cr_assert_eq(atomic_load(&probe.received_arg), (void*)&marker);

  list_destroy(sockets);
  socket_destroy(client);
}

Test(watchdog, does_not_ping_while_someone_else_holds_the_sockets_mutex)
{
  mtx_t sockets_mutex;
  mtx_init(&sockets_mutex, mtx_plain);

  t_socket* server;
  t_socket* client = connected_pair(&server, true);

  t_list* sockets = list_create();
  list_add(sockets, client);

  mtx_lock(&sockets_mutex);
  t_watchdog* watchdog = watchdog_start(sockets, nullptr, &sockets_mutex, 10,
                                        noop_on_disconnected, nullptr);
  cr_assert_not_null(watchdog);

  /* Several cycles' worth of time with the list locked: no pass may start. */
  time_sleep_ms(100);
  cr_assert_not(has_pending_data(server));

  /* Releasing the list lets the very next pass ping the socket. */
  mtx_unlock(&sockets_mutex);
  cr_assert_eq(receive_op_code(server), OP_CONNECTION_CHECK);
  free(receive_string(server));

  watchdog_destroy(watchdog);
  mtx_destroy(&sockets_mutex);
  list_destroy(sockets);
  socket_destroy(client);
  socket_destroy(server);
}

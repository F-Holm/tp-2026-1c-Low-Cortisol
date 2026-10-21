#include "utils/watchdog.h"

#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>
#include <threads.h>

#include "utils/collections/list.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/time.h"

static int watch_sockets(void* args);
static void check_all_sockets(t_watchdog* watchdog);
static bool ping_socket(t_socket* socket);

t_watchdog* watchdog_start(t_list* sockets, mtx_t* sockets_mutex,
                           unsigned long cycle_interval_ms,
                           void (*on_disconnected)(void* arg),
                           void* callback_arg)
{
  t_watchdog* watchdog = malloc(sizeof(t_watchdog));
  watchdog->sockets = sockets;
  watchdog->sockets_mutex = sockets_mutex;
  watchdog->cycle_interval_ms = cycle_interval_ms;
  watchdog->on_disconnected = on_disconnected;
  watchdog->callback_arg = callback_arg;
  atomic_init(&(watchdog->close), false);

  if (thrd_create(&(watchdog->thread), watch_sockets, watchdog) != 0)
  {
    free(watchdog);
    return NULL;
  }
  return watchdog;
}

void watchdog_destroy(t_watchdog* watchdog)
{
  atomic_store(&(watchdog->close), true);
  thrd_join(watchdog->thread, NULL);
  free(watchdog);
}

static int watch_sockets(void* args)
{
  t_watchdog* watchdog = (t_watchdog*)args;

  while (!atomic_load(&(watchdog->close)))
  {
    time_sleep_ms(watchdog->cycle_interval_ms);
    check_all_sockets(watchdog);
  }
  return 0;
}

// One pass over the list, pinging every socket in it and reporting every
// failed ping through on_disconnected. sockets_mutex (if any) is held for
// the whole pass, matching the already-committed watchdogs' approach of
// holding their list/socket mutex while walking their connections.
static void check_all_sockets(t_watchdog* watchdog)
{
  if (watchdog->sockets_mutex != NULL)
  {
    mtx_lock(watchdog->sockets_mutex);
  }

  t_list_iterator* iterator = list_iterator_create(watchdog->sockets);
  while (list_iterator_has_next(iterator))
  {
    t_socket* socket = list_iterator_next(iterator);
    if (!ping_socket(socket))
    {
      watchdog->on_disconnected(watchdog->callback_arg);
    }
  }
  list_iterator_destroy(iterator);

  if (watchdog->sockets_mutex != NULL)
  {
    mtx_unlock(watchdog->sockets_mutex);
  }
}

// Locked with the socket's own mutex, so a ping never races a real
// read/write some other thread is doing over the same socket.
static bool ping_socket(t_socket* socket)
{
  socket_mutex_lock(socket);
  bool alive =
      send_string(OP_CONNECTION_CHECK, "connection-check ping", socket);
  socket_mutex_unlock(socket);
  return alive;
}

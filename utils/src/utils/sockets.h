#pragma once

#include <stdbool.h>

#include "utils/mutex.h"
#include "utils/sockets_linux.h"
// -- future Windows support: switch the include above between
//    "utils/sockets_linux.h" and a new "utils/sockets_windows.h" behind an
//    #ifdef _WIN32; both must define t_socket_handle. Every function below
//    keeps its name/signature -- no #ifdef needed here or in any caller
//    (msg.h included). Not implemented yet.

/**
 * @file
 * @brief Platform-agnostic TCP socket, optionally paired with a mutex for
 *        callers that share it across threads. Backed today by
 *        sockets_linux.c (BSD sockets over pthreads).
 */

/** @brief The role a socket_create() call is opening. */
typedef enum
{
  SOCKET_KIND_CLIENT,  // resolves + connects to ip:port (ip required)
  SOCKET_KIND_SERVER   // resolves + binds every local interface + listens
                       // (ip ignored, pass NULL)
} t_socket_kind;

typedef enum
{
  SOCKET_SHUTDOWN_READ,
  SOCKET_SHUTDOWN_WRITE,
  SOCKET_SHUTDOWN_BOTH
} t_socket_shutdown_mode;

// Port string meaning "let the OS pick a free port". Passed straight
// through as the service string, matching the "0"-string convention already
// used project-wide (every module's tests/support.c). Only meaningful for
// SOCKET_KIND_SERVER.
#define SOCKET_PORT_EPHEMERAL "0"

/** @brief A socket with an optional mutex for callers sharing it across
 * threads. */
typedef struct
{
  t_socket_handle handle;
  mtx_t mutex;     // always initialized; see has_mutex
  bool has_mutex;  // whether socket_mutex_lock/unlock actually lock it
} t_socket;

/**
 * @brief Creates a socket. See t_socket_kind for what each kind does.
 * @param ip          Required for SOCKET_KIND_CLIENT; ignored (pass NULL)
 *                    for SOCKET_KIND_SERVER.
 * @param with_mutex  Whether to allocate a mutex for this socket (see
 *                    socket_mutex_lock/unlock below).
 * @return A new socket, or NULL on failure (unresolvable address, connect()
 *         failure, bind() failure, ...).
 */
t_socket* socket_create(t_socket_kind kind, char* ip, char* port,
                        bool with_mutex);

/**
 * @brief Accepts one pending connection on @p listener (a SOCKET_KIND_SERVER
 *        socket). Blocks until a peer connects.
 * @return A new socket for the accepted connection, or NULL on failure.
 */
t_socket* socket_accept(t_socket* listener, bool with_mutex);

/** @brief Sends exactly @p size bytes of @p data. @return false if @p socket is
 * NULL/closed or the peer is gone. */
bool socket_send(t_socket* socket, const void* data, int size);

/** @brief Reads exactly @p size bytes into @p data (blocks until all arrive).
 * @return false if @p socket is NULL/closed, the peer closed, or an error
 * occurred. */
bool socket_receive(t_socket* socket, void* data, int size);

/** @brief The local port @p socket is bound to (e.g. after
 * SOCKET_PORT_EPHEMERAL). @return The port, or -1 on failure. */
int socket_get_local_port(t_socket* socket);

/** @brief Shuts down @p socket's handle for further read/write/both, without
 * closing it. No-op if @p socket is NULL. */
void socket_shutdown(t_socket* socket, t_socket_shutdown_mode mode);

/** @brief Closes @p socket's underlying OS handle. Does not free @p socket or
 * its mutex -- see socket_destroy(). Safe to call with socket == NULL. */
void socket_close(t_socket* socket);

/** @brief Closes @p socket (if not already), destroys its mutex if it has one,
 * and frees @p socket. Safe to call with socket == NULL. */
void socket_destroy(t_socket* socket);

/**
 * @brief Locks/unlocks @p socket's mutex. No-op if it has none. Always
 *        caller-driven -- no function in this project locks/unlocks a
 *        socket's mutex except these two.
 */
void socket_mutex_lock(t_socket* socket);
void socket_mutex_unlock(t_socket* socket);

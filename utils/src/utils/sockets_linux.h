#pragma once

#include <stdbool.h>

// Linux backing type and primitives for utils/sockets.h. A future Windows
// backend would define the same names (e.g. over SOCKET) in
// sockets_windows.h, included from sockets.c behind an #ifdef _WIN32
// instead of this file.
//
// This header intentionally knows nothing about t_socket/t_socket_kind/
// mtx_t (all defined in sockets.h): it only deals in raw handles, so it
// never needs to include sockets.h, and sockets.h including it back (for
// t_socket_handle) can never form a cycle. All of the platform-agnostic
// glue (malloc'ing a t_socket, allocating its mutex, picking connect vs.
// listen for a given t_socket_kind) lives in sockets.c instead, which is
// the only file that includes both this header and sockets.h.
//
// -1 is Linux's "invalid fd" sentinel, exposed only through
// socket_handle_is_valid() -- nothing outside this layer inspects the raw
// value.
typedef int t_socket_handle;

/** @brief A handle that is guaranteed never to be returned by a successful
 * open/accept. */
t_socket_handle socket_handle_invalid(void);
bool socket_handle_is_valid(t_socket_handle handle);

/** @brief Opens a TCP connection to @p ip : @p port. @return An invalid handle
 * on failure. */
t_socket_handle socket_handle_connect(char* ip, char* port);

/** @brief Creates a listening TCP socket bound to @p port (every local
 * interface). */
t_socket_handle socket_handle_listen(char* port);

/** @brief Accepts one pending connection on @p listener. Blocks until a peer
 * connects. @return An invalid handle on failure. */
t_socket_handle socket_handle_accept(t_socket_handle listener);

bool socket_handle_send(t_socket_handle handle, const void* data, int size);
bool socket_handle_receive(t_socket_handle handle, void* data, int size);

/** @return The local port @p handle is bound to, or -1 on failure. */
int socket_handle_get_local_port(t_socket_handle handle);

/** @brief Shuts down @p handle for reading, writing, or both, without closing
 * it. */
void socket_handle_shutdown(t_socket_handle handle, bool disable_read,
                            bool disable_write);

void socket_handle_close(t_socket_handle handle);

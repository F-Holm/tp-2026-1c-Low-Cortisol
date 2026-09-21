#pragma once

// Linux backing type for utils/sockets.h's t_socket: a plain file
// descriptor. A future Windows backend would define the same name (e.g.
// over SOCKET) in sockets_windows.h, included from sockets.h behind an
// #ifdef _WIN32 instead of this file.
//
// -1 is Linux's "invalid fd" sentinel; it is compared against only inside
// sockets_linux.c -- nothing outside that file ever inspects the raw value.
typedef int t_socket_handle;

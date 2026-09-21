#pragma once

/**
 * @file
 * @brief Detects the target operating system at compile time.
 *
 * Defines exactly one of OS_LINUX / OS_WINDOWS (with no value); test it with
 * `#ifdef`. Meant for picking which backing implementation (threads_*,
 * mutex_*, sockets_*) to include; nothing else should depend on it.
 */

#if defined(_WIN32)
#define OS_WINDOWS
#elif defined(__linux__)
#define OS_LINUX
#else
#error "Unsupported operating system: utils only supports Linux and Windows"
#endif

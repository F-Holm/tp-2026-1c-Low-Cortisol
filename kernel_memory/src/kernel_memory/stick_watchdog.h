#pragma once

#include "kernel_memory/structs.h"
#include "utils/watchdog.h"

// Periodically pings every connected memory stick so a disconnection is
// noticed even if no process happens to be using that stick at the time
// (e.g. a process stuck in a CPU-bound loop that never touches memory).
// A thin wrapper over utils/watchdog.h: it pings straight off
// kernel_data->connected_sticks/socket_list_mutex (no shadow list), and
// notifies the Kernel Scheduler (once) when a stick goes unreachable.

/** @brief Starts the periodic stick connection-check thread. */
t_watchdog* start_stick_watchdog(t_kernel_memory_data* kernel_data);

/** @brief Stops the watchdog thread and frees it. */
void destroy_stick_watchdog(t_watchdog* watchdog);

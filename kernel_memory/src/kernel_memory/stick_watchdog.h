#pragma once

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "kernel_memory/structs.h"

// Periodically pings every connected memory stick so a disconnection is
// noticed even if no process happens to be using that stick at the time
// (e.g. a process stuck in a CPU-bound loop that never touches memory).
// Mirrors kernel_scheduler's connection-check thread for its Kernel Memory
// connection.
typedef struct
{
  t_kernel_memory_data* kernel_data;
  pthread_t thread;
  atomic_bool close;
  bool already_notified;
} t_stick_watchdog;

t_stick_watchdog* start_stick_watchdog(t_kernel_memory_data* kernel_data);
void destroy_stick_watchdog(t_stick_watchdog* watchdog);

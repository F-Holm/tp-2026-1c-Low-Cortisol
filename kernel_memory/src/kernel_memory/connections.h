#pragma once

#include <pthread.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"

// Handshake steps: read the peer's self-description off the socket. False
// (and the connection closed) if the peer sent something else.
bool receive_cpu_id(t_cpu_data* cpu_data);
bool receive_stick_size(t_stick_data* stick_data);
bool receive_stick_listen_port(t_stick_data* stick_data);

void add_stick_connection(t_kernel_memory_data* kernel_data,
                          t_stick_data* stick_data);
void add_cpu_connection(t_kernel_memory_data* kernel_data,
                        t_cpu_data* cpu_data);

// Tell a newly connected CPU about every Memory Stick already online, and tell
// every connected CPU about a newly connected stick.
void send_connected_sticks(t_list* connected_sticks,
                           pthread_mutex_t* socket_list_mutex,
                           t_cpu_data* cpu_data);
void send_cpu_connection(t_stick_data* stick_data, t_list* connected_cpus);

int compute_total_memory(t_list* connected_sticks,
                         pthread_mutex_t* socket_list_mutex);

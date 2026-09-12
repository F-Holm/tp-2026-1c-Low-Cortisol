#pragma once

#include "cpu/cpu.h"

// The control-plane exchanges with Kernel Memory: the startup handshake
// (max segment size) and the in-band messages that can arrive between
// instruction cycles (a newly connected Memory Stick).
bool receive_max_segment_size(t_cpu* cpu);
bool parse_stick_packet(t_cpu* cpu, t_list* packet, char stick_ip[16],
                        char stick_port[6], uint32_t* size);
bool listen_kernel_memory(t_cpu* cpu);

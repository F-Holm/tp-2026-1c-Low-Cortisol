#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cpu/cpu.h"
#include "utils/collections/list.h"

// The control-plane exchanges with Kernel Memory: the startup handshake
// (max segment size) and the in-band messages that can arrive between
// instruction cycles (a newly connected Memory Stick).
/**
 * @brief Receives and stores the max segment size from Kernel Memory
 *        (startup handshake).
 * @return false on a wrong op code.
 */
bool receive_max_segment_size(t_cpu* cpu);

/**
 * @brief Parses a Memory Stick's [ip, port, size] @p packet into the
 *        out-params.
 * @note Frees @p packet (and its elements) either way.
 * @return false if the packet doesn't have exactly 3 fields.
 */
bool parse_stick_packet(t_cpu* cpu, t_list* packet, char stick_ip[16],
                        char stick_port[6], uint32_t* size);

/**
 * @brief Waits for a reply from Kernel Memory, transparently handling any
 *        OP_PACKET (new Memory Stick) received in between.
 * @return false if Kernel Memory disconnected or sent something unexpected.
 */
bool listen_kernel_memory(t_cpu* cpu);

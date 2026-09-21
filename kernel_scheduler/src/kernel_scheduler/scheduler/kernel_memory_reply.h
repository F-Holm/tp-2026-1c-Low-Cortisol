#pragma once

#include "kernel_scheduler/scheduler/queue_types.h"

/**
 * @file
 * @brief Central handling of the op codes Kernel Memory sends the scheduler.
 *
 * Besides the reply to a request, Kernel Memory can send two notifications at
 * any time: OP_NEW_MEMORY_STICK (any number of them may pile up on the socket
 * before the reply) and OP_MEMORY_CORRUPTED. receive_km_opcode() deals with
 * both so no caller has to.
 */

/**
 * @brief Receives the next op code Kernel Memory sends and checks it is one of
 *        @p expected.
 *
 * - An expected op code is returned as is. Its payload is left unread; the
 *   caller reads it.
 * - OP_NEW_MEMORY_STICK: its payload is discarded, the resumption routine is
 *   started, and the next op code is received.
 * - OP_MEMORY_CORRUPTED, a closed connection, or any other op code: the
 *   scheduler is shut down (see close_kernel_scheduler()) and OP_CODE_ERROR is
 *   returned.
 *
 * @param expected  Op codes the caller accepts as the reply.
 * @param count     Number of entries in @p expected.
 * @return The received expected op code, or OP_CODE_ERROR once the scheduler
 *         has been shut down.
 * @note The caller must hold the Kernel Memory socket's mutex.
 */
int receive_km_opcode(t_queues* queues, const int* expected, int count);

/**
 * @brief receive_km_opcode() with the expected op codes listed inline:
 *        `RECEIVE_KM_OPCODE(queues, OP_SUSPENSION_OK, OP_SUSPENSION_FAILED)`.
 */
#define RECEIVE_KM_OPCODE(queues, ...)                    \
  receive_km_opcode((queues), (const int[]){__VA_ARGS__}, \
                    (int)(sizeof((const int[]){__VA_ARGS__}) / sizeof(int)))

#pragma once

#include <assert.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/collections/list.h"
#include "utils/log.h"

// Operation codes are serialized as ints on the wire: keep the order stable.
typedef enum
{
  OP_CODE_ERROR,
  OP_HANDSHAKE,
  OP_PACKET,
  OP_PORT,
  OP_ID_CPU,
  OP_INFO_SWAP,
  OP_MEMORY_SIZE,
  OP_IO_TYPE,
  OP_IO_STDIN_REQUEST,
  OP_IO_STDOUT_REQUEST,
  OP_IO_SLEEP_REQUEST,
  OP_STDIN_RESPONSE,
  OP_STDOUT_RESPONSE,
  OP_SLEEP_RESPONSE,
  OP_MEMORY_CORRUPTED,
  OP_CPU_CYCLE_OK,
  OP_SEG_FAULT,
  OP_SYSCALL_MUTEX_CREATE,  // Do not change the order of the syscalls
  OP_SYSCALL_MUTEX_LOCK,    // Do not add entries between the syscalls
  OP_SYSCALL_MUTEX_UNLOCK,
  OP_SYSCALL_MEM_ALLOC,
  OP_SYSCALL_MEM_FREE,
  OP_SYSCALL_SLEEP,
  OP_SYSCALL_STDOUT,
  OP_SYSCALL_STDIN,
  OP_SYSCALL_INIT_PROC,
  OP_SYSCALL_EXIT,
  OP_RESUME_PROCESS,
  OP_NEW_PROCESS,  // No reply
  OP_END_PROCESS,  // No reply
  OP_NEXT_INSTRUCTION,
  OP_SEND_INSTRUCTION,
  OP_REQUEST_CONTEXT,
  OP_SEND_CONTEXT,
  OP_UPDATED_CONTEXT,
  OP_NO_INTERRUPT,
  OP_INTERRUPT,
  OP_REQUEST_FREE_MEMORY,
  OP_FREE_MEMORY,
  OP_REQUEST_PROCESS_SIZE,  // process may be suspended
  OP_PROCESS_SIZE,          // process size in memory
  OP_NEW_MEMORY_STICK,      // a memory stick connected
  OP_SUSPEND_PROCESS,
  OP_SUSPENSION_OK,
  OP_SUSPENSION_FAILED,
  OP_RESUME_SUSPENDED_PROCESS,  // replies with one of the next two
  OP_RESUME_SUSPENSION_OK,
  OP_RESUME_SUSPENSION_FAILED,
  OP_COMPACTION_NEEDED,
  OP_CAN_COMPACT,  // compaction may start
  OP_COMPACTION_DONE,
  OP_MEMORY_ALLOCATED,
  OP_MEMORY_FREED,
  OP_SEGMENT_SIZE_EXCEEDED,
  OP_MAX_SEGMENT_SIZE,
  OP_MEMORY_STICK_READ,
  OP_MEMORY_STICK_WRITE,
  OP_MEMORY_STICK_READ_DONE,
  OP_MEMORY_STICK_WRITE_DONE,
  OP_SEGMENT_TABLE,
  OP_NOT_ENOUGH_MEMORY,
  OP_KERNEL_SCHEDULER_SHUTDOWN,
  OP_UPDATED_SEGMENT_TABLE,
  OP_DISK_WRITE,
  OP_DISK_READ,
  OP_DISK_WRITE_DONE,
  OP_DISK_READ_DONE,
  OP_PROCESS_STARTED,
  OP_STICK_DISCONNECTED,
  OP_KERNEL_MEMORY_RUNNING
} t_op_code;

typedef struct
{
  int size;
  void* stream;
} t_buffer;

typedef struct
{
  t_op_code op_code;
  t_buffer* buffer;
} t_packet;

typedef enum
{
  MID_KERNEL_SCHEDULER,
  MID_KERNEL_MEMORY,
  MID_CPU,
  MID_MEMORY_STICK,
  MID_SWAP,
  MID_IO,
  MID_MODULE_ID_ERROR
} t_module_id;

extern const char* const HANDSHAKE_MSG[6];

/**
 * @brief Opens a TCP connection to @p ip : @p port.
 * @return The connected socket fd, or -1 on failure.
 */
int create_connection(char* ip, char* port);

/**
 * @brief Creates a listening TCP socket bound to @p port (INADDR_ANY).
 * @return The listening socket fd.
 */
int start_server(char* port);

/**
 * @brief Receives the operation code.
 * @note Always call this before any receive/read from the buffer.
 */
int receive_op_code(int socket_fd);

/**
 * @brief Receives raw data from the buffer.
 * @param size  Out-param: number of bytes read.
 * @note Call after receive_op_code(). Free the returned buffer.
 */
void* receive_buffer(int* size, int socket_fd);

/**
 * @brief Sends a void* payload with the given operation code.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_buffer(int op_code, void* buffer, int size, int socket_fd);

/**
 * @brief Sends a char* payload with the given operation code.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_string(int op_code, char* message, int socket_fd);

/**
 * @brief Receives a char*.
 * @note Call after receive_op_code(). Free the returned string.
 */
char* receive_string(int socket_fd);

/**
 * @brief Maps a HANDSHAKE_MSG[] string to its t_module_id.
 * @note Call after receive_string().
 */
int handshake_msg_to_module_id(char* handshake_msg);

/**
 * @brief Sends a handshake for @p module_id.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_handshake(int module_id, int socket_fd);

/** @brief Receives a handshake and returns the sender's t_module_id. */
int receive_handshake(int socket_fd);

/**
 * @brief Creates a packet with the given operation code.
 * @note Free it with destroy_packet().
 */
t_packet* create_packet(int op_code);

/**
 * @brief Appends @p size bytes of @p value to the packet.
 * @note Call after create_packet().
 */
void packet_append(t_packet* packet, void* value, int size);

/**
 * @brief Appends a string (including its '\0') to the packet.
 * @note Call after create_packet().
 */
void packet_append_string(t_packet* packet, char* value);

/**
 * @brief Sends the packet.
 * @return false if nothing was sent or the peer disconnected.
 */
bool send_packet(t_packet* packet, int socket_fd);

/**
 * @brief Receives a packet as a list of void* elements.
 * @note Free the list and every element after use. Strings added with
 *       packet_append_string() are already '\0'-terminated.
 */
t_list* receive_packet(int socket_fd);

/** @brief Destroys the packet. */
void destroy_packet(t_packet* packet);

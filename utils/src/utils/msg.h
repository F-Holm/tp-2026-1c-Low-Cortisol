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

// Wire operation codes. Serialized as a plain int; every module is built from
// this header, so the numeric values are an internal detail. Grouped by the
// module pair that exchanges them -- "A -> B" means A sends it, B receives it.
typedef enum
{
  // ==== Common ====
  OP_CODE_ERROR,  // receive_op_code() result when the socket is closed
  OP_HANDSHAKE,   // module identification, on every new connection

  // ==== Kernel Scheduler <-> CPU ====

  // CPU -> Kernel Scheduler / Kernel Memory / Memory Stick (on connect)
  OP_ID_CPU,

  // Kernel Scheduler -> CPU
  OP_RESUME_PROCESS,  // run this PID (also: resume it after a syscall)
  OP_INTERRUPT,       // preempt the running process (reason as a string)
  OP_NO_INTERRUPT,    // keep executing

  // CPU -> Kernel Scheduler. Dispatched by value: keep this block contiguous
  // and in this order -- kernel_scheduler/cpu.c indexes a handler table by
  // (op_code - OP_CPU_CYCLE_OK), and SYSCALL_NAMES by
  // (op_code - OP_SYSCALL_MUTEX_CREATE).
  OP_CPU_CYCLE_OK,  // instruction cycle finished, no syscall
  OP_SEG_FAULT,     // segmentation fault, finish the process
  OP_SYSCALL_MUTEX_CREATE,
  OP_SYSCALL_MUTEX_LOCK,
  OP_SYSCALL_MUTEX_UNLOCK,
  OP_SYSCALL_MEM_ALLOC,
  OP_SYSCALL_MEM_FREE,
  OP_SYSCALL_SLEEP,
  OP_SYSCALL_STDOUT,
  OP_SYSCALL_STDIN,
  OP_SYSCALL_INIT_PROC,
  OP_SYSCALL_EXIT,

  // ==== Kernel Scheduler <-> Kernel Memory ====

  // Kernel Scheduler -> Kernel Memory
  OP_KERNEL_MEMORY_RUNNING,      // connection-check ping
  OP_NEW_PROCESS,                // create a process (PID + path); no reply
  OP_END_PROCESS,                // terminate a process (PID); no reply
  OP_CREATE_SEGMENT,             // MEM_ALLOC: create a process segment
  OP_DELETE_SEGMENT,             // MEM_FREE: remove a process segment
  OP_REQUEST_FREE_MEMORY,        // ask for the current free space
  OP_REQUEST_PROCESS_SIZE,       // ask for a process's size in memory
  OP_SUSPEND_PROCESS,            // move a process's segments to swap
  OP_RESUME_SUSPENDED_PROCESS,   // restore a process's segments from swap
  OP_CAN_COMPACT,                // CPUs preempted, compaction may start
  OP_KERNEL_SCHEDULER_SHUTDOWN,  // the scheduler is shutting down

  // Kernel Memory -> Kernel Scheduler
  OP_PROCESS_STARTED,        // process creation confirmed
  OP_FREE_MEMORY,            // free-space value
  OP_PROCESS_SIZE,           // process size in memory
  OP_MEMORY_ALLOCATED,       // MEM_ALLOC succeeded
  OP_MEMORY_FREED,           // MEM_FREE done
  OP_SEGMENT_SIZE_EXCEEDED,  // requested segment larger than the max
  OP_NOT_ENOUGH_MEMORY,      // not enough free space for the segment
  OP_COMPACTION_NEEDED,      // compaction required, preempt every CPU
  OP_COMPACTION_DONE,        // compaction finished
  OP_SUSPENSION_OK,
  OP_SUSPENSION_FAILED,
  OP_RESUME_SUSPENSION_OK,
  OP_RESUME_SUSPENSION_FAILED,
  OP_NEW_MEMORY_STICK,  // a stick connected, more memory available
  OP_MEMORY_CORRUPTED,  // a stick disconnected -> BSOD

  // ==== Kernel Scheduler <-> IO ====

  // IO -> Kernel Scheduler (STDIN / STDOUT / SLEEP, on connect)
  OP_IO_TYPE,

  // Kernel Scheduler -> IO (the request opcodes are also reused
  // Kernel Scheduler -> Kernel Memory for the data half of the transfer)
  OP_IO_STDIN_REQUEST,
  OP_IO_STDOUT_REQUEST,
  OP_IO_SLEEP_REQUEST,

  // IO -> Kernel Scheduler (the STDIN / STDOUT responses are also reused
  // Kernel Memory -> Kernel Scheduler)
  OP_STDIN_RESPONSE,
  OP_STDOUT_RESPONSE,
  OP_SLEEP_RESPONSE,

  // ==== CPU <-> Kernel Memory ====

  // CPU -> Kernel Memory
  OP_REQUEST_CONTEXT,        // fetch a PID's execution context
  OP_UPDATED_CONTEXT,        // push the updated context back
  OP_NEXT_INSTRUCTION,       // fetch the instruction at the program counter
  OP_UPDATED_SEGMENT_TABLE,  // ask for a refreshed segment table
  OP_STICK_DISCONNECTED,     // CPU noticed a memory stick dropped

  // Kernel Memory -> CPU
  OP_SEND_CONTEXT,      // the execution context
  OP_SEND_INSTRUCTION,  // the instruction
  OP_SEGMENT_TABLE,     // the process's segment table
  OP_MAX_SEGMENT_SIZE,  // max segment size (once, at CPU startup)
  OP_PACKET,            // connected memory-stick list (ip / port / size)

  // ==== CPU / Kernel Memory <-> Memory Stick ====

  // Memory Stick -> Kernel Memory (on connect)
  OP_MEMORY_SIZE,  // its size
  OP_PORT,         // its CPU-listen port

  // CPU / Kernel Memory -> Memory Stick
  OP_MEMORY_STICK_READ,
  OP_MEMORY_STICK_WRITE,

  // Memory Stick -> CPU / Kernel Memory
  OP_MEMORY_STICK_READ_DONE,
  OP_MEMORY_STICK_WRITE_DONE,

  // ==== Kernel Memory <-> SWAP ====

  // SWAP -> Kernel Memory (block size + total size, on connect)
  OP_INFO_SWAP,

  // Kernel Memory -> SWAP
  OP_DISK_WRITE,
  OP_DISK_READ,

  // SWAP -> Kernel Memory
  OP_DISK_WRITE_DONE,
  OP_DISK_READ_DONE
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

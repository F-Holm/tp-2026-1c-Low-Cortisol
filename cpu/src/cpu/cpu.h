#pragma once

#include <pthread.h>
#include <stdio.h>

#include "cpu/registers.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"

typedef struct
{
  char* id;
  int socket_kernel_memory;
  int socket_kernel_scheduler;
  uint32_t max_segment_size;

  t_list* memory_sticks;
  t_dictionary* handlers;
  t_log* logger;
  t_config* config;
} t_cpu;

typedef struct
{
  char* name;
  char* parameters[3];
  int parameter_count;
} t_instruction;

typedef struct
{
  int socket_ms;
  uint32_t size;
  uint32_t offset;
} t_memory_stick_info;

// A boolean plus the two out-of-band results the instruction cycle needs:
// EB_ERROR (fatal, abort) and EB_NO_TABLE (kernel memory reported the segment
// table is gone).
typedef enum
{
  EB_FALSE,
  EB_TRUE,
  EB_ERROR,
  EB_NO_TABLE
} t_extended_bool;

/** @brief Runs CPU instruction cycles until the Kernel Scheduler disconnects.
 */
void run_instruction_loop(t_cpu* cpu);

/** @brief Receives the PID to run next. @return The PID, or UINT32_MAX on
 * disconnect/error. */
uint32_t receive_pid(t_cpu* cpu);

/** @brief Requests @p pid's execution context from Kernel Memory. @return false
 * on failure. */
bool request_context(t_cpu* cpu, uint32_t pid);

/**
 * @brief Receives the execution context registers.
 * @note Call after listen_kernel_memory(). Free the returned pointer.
 */
t_registers* receive_context(t_cpu* cpu);

/**
 * @brief Receives @p pid's segment table, replacing @p context's current one.
 * @note Frees the previous segment_table held in @p context.
 */
t_list* receive_segment_table(t_cpu* cpu, t_context* context);

/**
 * @brief Runs fetch-decode-execute cycles for @p pid until interrupted.
 * @return false on a communication error.
 */
bool run_instruction_cycle(t_cpu* cpu, uint32_t pid, t_context* context);

/**
 * @brief Requests and receives the instruction at @p pc.
 * @return The raw instruction string, or NULL on failure. Caller frees it.
 */
char* fetch_stage(t_cpu* cpu, uint32_t pid, uint32_t pc);

/** @brief Requests the instruction at @p pc from Kernel Memory. @return false
 * on failure. */
bool request_instruction(t_cpu* cpu, uint32_t pid, uint32_t pc);

/** @brief Receives the raw instruction string. Caller frees it. */
char* receive_instruction(t_cpu* cpu);

/**
 * @brief Parses @p raw_instruction into a name and parameters.
 * @note Free the result with destroy_instruction().
 */
t_instruction* decode_stage(char* raw_instruction);

/** @brief Dispatches @p instruction to its registered handler. */
t_extended_bool execute_stage(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);

/**
 * @brief Waits for the Kernel Scheduler's interrupt decision after a cycle.
 * @return EB_TRUE to keep running, EB_FALSE/EB_NO_TABLE to stop, EB_ERROR on
 *         a communication error.
 */
t_extended_bool check_interrupt(t_cpu* cpu, uint32_t pid);

/** @brief Sends @p pid's updated registers to Kernel Memory. @return false on
 * failure. */
bool send_updated_context(t_cpu* cpu, uint32_t pid,
                          t_registers* updated_context);

/** @brief Requests a refreshed segment table for @p pid and updates @p context.
 * @return false on failure. */
bool update_segment_table(t_cpu* cpu, uint32_t pid, t_context* context);

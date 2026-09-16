#pragma once

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/registers_cpu.h"

typedef t_extended_bool (*t_handler)(t_cpu*, t_context*, t_instruction*,
                                     uint32_t);

/** @brief NOOP instruction: does nothing. */
t_extended_bool handler_noop(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid);

/** @brief SET instruction: sets register @p parameters[0] to the value in @p
 * parameters[1]. */
t_extended_bool handler_set(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);

/** @brief SUM instruction: adds register @p parameters[1] into register @p
 * parameters[0]. */
t_extended_bool handler_sum(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);

/** @brief SUB instruction: subtracts register @p parameters[1] from register @p
 * parameters[0]. */
t_extended_bool handler_sub(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);

/** @brief JNZ instruction: jumps to @p parameters[1] if register @p
 * parameters[0] is non-zero. */
t_extended_bool handler_jnz(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);

/** @brief MOV_IN instruction: reads memory at the SI-derived address into
 * register @p parameters[0]. */
t_extended_bool handler_mov_in(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid);

/** @brief MOV_OUT instruction: writes register @p parameters[0] to memory at
 * the DI-derived address. */
t_extended_bool handler_mov_out(t_cpu* cpu, t_context* context,
                                t_instruction* instruction, uint32_t pid);

/** @brief COPY_MEM instruction: copies @p parameters[0] bytes from the SI to
 * the DI address. */
t_extended_bool handler_copy_mem(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid);

/**
 * @brief MUTEX_CREATE / MUTEX_LOCK / MUTEX_UNLOCK instructions: forward the
 *        corresponding mutex syscall, named in @p parameters[0], to the
 *        Kernel Scheduler.
 */
t_extended_bool handler_mutex_create(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mutex_lock(t_cpu* cpu, t_context* context,
                                   t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mutex_unlock(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid);

/** @brief MEM_ALLOC instruction: requests a segment of size @p parameters[1]
 * with id @p parameters[0]. */
t_extended_bool handler_mem_alloc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid);

/** @brief MEM_FREE instruction: requests freeing the segment with id @p
 * parameters[0]. */
t_extended_bool handler_mem_free(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid);

/** @brief SLEEP instruction: requests blocking the process for @p parameters[0]
 * ms. */
t_extended_bool handler_sleep(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);

/** @brief STDOUT instruction: requests writing the memory at register @p
 * parameters[0] to stdout. */
t_extended_bool handler_stdout(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid);

/** @brief STDIN instruction: requests reading input into the memory at register
 * @p parameters[0]. */
t_extended_bool handler_stdin(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);

/** @brief INIT_PROC instruction: requests creating a process from file @p
 * parameters[0] with priority @p parameters[1]. */
t_extended_bool handler_init_proc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid);

/** @brief EXIT instruction: requests terminating the current process. */
t_extended_bool handler_exit(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid);

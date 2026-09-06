#pragma once

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/registros_cpu.h"

typedef t_extended_bool (*t_handler)(t_cpu*, t_context*, t_instruction*,
                                     uint32_t);

t_extended_bool handler_noop(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid);
t_extended_bool handler_set(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);
t_extended_bool handler_sum(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);
t_extended_bool handler_sub(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);
t_extended_bool handler_jnz(t_cpu* cpu, t_context* context,
                            t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mov_in(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mov_out(t_cpu* cpu, t_context* context,
                                t_instruction* instruction, uint32_t pid);
t_extended_bool handler_copy_mem(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mutex_create(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mutex_lock(t_cpu* cpu, t_context* context,
                                   t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mutex_unlock(t_cpu* cpu, t_context* context,
                                     t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mem_alloc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid);
t_extended_bool handler_mem_free(t_cpu* cpu, t_context* context,
                                 t_instruction* instruction, uint32_t pid);
t_extended_bool handler_sleep(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);
t_extended_bool handler_stdout(t_cpu* cpu, t_context* context,
                               t_instruction* instruction, uint32_t pid);
t_extended_bool handler_stdin(t_cpu* cpu, t_context* context,
                              t_instruction* instruction, uint32_t pid);
t_extended_bool handler_init_proc(t_cpu* cpu, t_context* context,
                                  t_instruction* instruction, uint32_t pid);
t_extended_bool handler_exit(t_cpu* cpu, t_context* context,
                             t_instruction* instruction, uint32_t pid);

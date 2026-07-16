#ifndef CPU_HANDLERS_H_
#define CPU_HANDLERS_H_

#include <stdio.h>

#include "cpu/cpu.h"
#include "utils/registros.h"

typedef bool (*t_handler)(t_cpu*, t_contexto*, t_instruccion*, uint32_t);

bool handler_noop(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                  uint32_t pid);
bool handler_set(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid);
bool handler_sum(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid);
bool handler_sub(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid);
bool handler_jnz(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                 uint32_t pid);
t_bool_extendido handler_mov_in(t_cpu* cpu, t_contexto* contexto,
                    t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mov_out(t_cpu* cpu, t_contexto* contexto,
                     t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_copy_mem(t_cpu* cpu, t_contexto* contexto,
                      t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mutex_create(t_cpu* cpu, t_contexto* contexto,
                          t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mutex_lock(t_cpu* cpu, t_contexto* contexto,
                        t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mutex_unlock(t_cpu* cpu, t_contexto* contexto,
                          t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mem_alloc(t_cpu* cpu, t_contexto* contexto,
                       t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_mem_free(t_cpu* cpu, t_contexto* contexto,
                      t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_sleep(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid);
t_bool_extendido handler_stdout(t_cpu* cpu, t_contexto* contexto,
                    t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_stdin(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                   uint32_t pid);
t_bool_extendido handler_init_proc(t_cpu* cpu, t_contexto* contexto,
                       t_instruccion* instruccion, uint32_t pid);
t_bool_extendido handler_exit(t_cpu* cpu, t_contexto* contexto, t_instruccion* instruccion,
                  uint32_t pid);

#endif /* CPU_HANDLERS_H_ */

#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registers_cpu.h"

/** @brief Closes @p cpu's sockets, frees its resources and the struct itself.
 */
void close_module(t_cpu* cpu);

/** @brief Frees an instruction's name, parameters and the struct itself. */
void destroy_instruction(t_instruction* instruction);

/** @brief Closes the stick's socket and frees it. Element destroyer for t_list.
 */
void destroy_memory_stick(void* value);

/** @brief Closes the socket fd pointed to by @p value and frees it. Element
 * destroyer for t_list. */
void iterator_close_socket(void* value);

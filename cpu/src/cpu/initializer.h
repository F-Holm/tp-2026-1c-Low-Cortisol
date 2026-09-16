#pragma once

#include "cpu/cpu.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/registers_cpu.h"

/**
 * @brief Loads @p cpu's config and logger from @p config_path and creates
 *        its memory_sticks list.
 * @return false on failure.
 */
bool init_module(t_cpu* cpu, char* config_path);

/** @brief Validates argc/argv (expects [config] [id]), printing usage on
 * failure. */
bool check_arguments(int argc, char** argv);

/** @brief Registers every instruction handler by name in @p handlers. */
void register_handlers(t_dictionary* handlers);

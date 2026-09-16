#pragma once

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "kernel_memory/structs.h"
#include "utils/config.h"
#include "utils/log.h"

/** @brief Creates the module's logger. */
t_log* init_logger(t_config* config);

/** @brief Loads the config file at @p path. */
t_config* init_config(char* path);

/** @brief Closes a socket. */
void close_communication(int client_socket);

/** @brief Reads SCRIPTS_BASEPATH from the config. */
char* get_scripts_basepath(t_config* config);

/** @brief Reads INSTRUCTION_DELAY, COMPACTION_DELAY and SEGMENT_MAX_SIZE
 *         from the config, respectively. */
int get_instruction_delay(t_config* config);
int get_compaction_delay(t_config* config);
int get_segment_max_size(t_config* config);

/** @brief Reads and parses ALLOCATION_STRATEGY from the config. */
t_allocation_strategy get_allocation_strategy(t_config* config);

/**
 * @brief Parses "BEST"/"WORST" into a t_allocation_strategy.
 * @return -1 if @p strategy is neither.
 */
t_allocation_strategy allocation_from_string(char* strategy);

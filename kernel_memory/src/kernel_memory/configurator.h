#pragma once

#include <pthread.h>
#include <string.h>
#include <unistd.h>

#include "kernel_memory/structs.h"
#include "utils/config.h"
#include "utils/log.h"

t_log* init_logger(t_config* config);
t_config* init_config(char* path);
void close_communication(int client_socket);
char* get_scripts_basepath(t_config* config);

int get_instruction_delay(t_config* config);
int get_compaction_delay(t_config* config);
int get_segment_max_size(t_config* config);
t_allocation_strategy get_allocation_strategy(t_config* config);
t_allocation_strategy allocation_from_string(char* strategy);

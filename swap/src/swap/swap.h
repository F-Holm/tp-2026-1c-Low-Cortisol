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
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/swap_km.h"

typedef struct
{
  t_log* logger;
  char* ip;
  char* port;
  int socket_swap;
  int swap_size;
  int block_size;
  char* swap_file_path;
  FILE* swap_file;
} t_swap;

void close_swap(t_swap* swap, t_config* config);
bool connect_to_kernel_memory(t_swap* swap, t_config* config);
bool init_config(t_swap* swap, t_config* config);
void write_block(FILE* swap_file, int block_number, int block_size,
                 char* content);
void read_block(FILE* swap_file, int block_number, int block_size,
                char* content);

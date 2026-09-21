#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  t_socket* socket_swap;
  int swap_size;
  int block_size;
  char* swap_file_path;
  FILE* swap_file;
} t_swap;

/**
 * @brief Releases whichever @p swap resources were already acquired and
 *        destroys @p config.
 * @note Safe to call on partial initialization; runs on every early-exit
 *       path.
 */
void close_swap(t_swap* swap, t_config* config);

/**
 * @brief Connects to Kernel Memory, handshakes and reports the SWAP geometry.
 * @return false if any step failed (@p config is destroyed via close_swap()).
 */
bool connect_to_kernel_memory(t_swap* swap, t_config* config);

/**
 * @brief Reads @p config into @p swap and creates the SWAP file.
 * @return false if any step failed (@p config is destroyed via close_swap()).
 */
bool init_config(t_swap* swap, t_config* config);

/** @brief Writes one block to the SWAP file. */
void write_block(FILE* swap_file, int block_number, int block_size,
                 char* content);

/** @brief Reads one block from the SWAP file into @p content. */
void read_block(FILE* swap_file, int block_number, int block_size,
                char* content);

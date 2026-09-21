#pragma once

#include <stdio.h>

#include "utils/config.h"
#include "utils/log.h"
#include "utils/mutex.h"
#include "utils/sockets.h"
#include "utils/threads.h"

typedef struct
{
  char* log_level;
  int memory_delay;
  char* km_ip;
  char* km_port;
} t_config_vars;

typedef struct
{
  t_config* config;
  t_log* logger;
  int memory_delay;
  t_socket* socket_km;
  t_socket* socket_server_cpu;
  char* memory;
  mtx_t* memory_mutex;
} t_ms;

/**
 * @brief Sends the CPU server's port (derived from @p socket_server_cpu) to
 *        Kernel Memory.
 * @return false on failure.
 */
bool send_cpu_server_port(t_socket* socket_km, t_socket* socket_server_cpu,
                          t_log* logger);

/**
 * @brief Loads the config, connects to Kernel Memory, reserves the stick's
 *        memory and starts the CPU server thread.
 * @return false if any step failed.
 */
bool init_module(t_ms* ms, char* config_path, char* size,
                 thrd_t* cpu_server_thread);

/**
 * @brief Loads the config file and fills @p config_vars.
 * @return The config, or NULL on failure.
 */
t_config* init_config(char* config_path, t_config_vars* config_vars);

/** @brief Creates the module's logger. */
t_log* init_logger(t_log_level log_level);

/** @brief Fills @p config_vars from @p config. */
void read_config(t_config* config, t_config_vars* config_vars);

/**
 * @brief Releases whichever @p ms resources were already acquired.
 * @note Safe to call with partially-initialized fields.
 */
void close_module_on_error(t_ms* ms);

/**
 * @brief Shuts down the CPU server, joins its thread and releases every
 *        @p ms resource.
 */
void close_module(t_ms* ms, thrd_t* cpu_server_thread);

/**
 * @brief Parses argv into @p config_path, @p size_str and @p size.
 * @return false if argc is wrong or the size isn't a positive number.
 */
bool get_args(int argc, char** argv, char** config_path, char** size_str,
              int* size);

/**
 * @brief Writes @p byte_count bytes at @p start_position and replies to
 *        @p dest_socket.
 */
void write_memory(t_ms* ms, int start_position, char* bytes_to_write,
                  int byte_count, t_socket* dest_socket);

/**
 * @brief Reads @p byte_count bytes from @p start_position and sends them
 *        to @p dest_socket.
 */
void read_memory(t_ms* ms, int start_position, int byte_count,
                 t_socket* dest_socket);

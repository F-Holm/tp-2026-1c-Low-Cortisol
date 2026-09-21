#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* port;
  t_socket* socket_io;
  int io_type;
  int sleep;
} t_io;

/** @brief Closes @p io's socket and destroys its logger and config. */
void close_io(t_io* io);

/**
 * @brief Connects to the Kernel Scheduler, handshakes and sends the IO type.
 * @return false on failure (also closes @p io via close_io()).
 */
bool connect_to_scheduler(t_io* io);

/** @brief Parses argv into @p io's config path and IO type. @return false if
 * invalid. */
bool parse_args(int argc, char** argv, t_io* io);

/** @brief Loads @p io's config values and creates its logger. @return false on
 * failure. */
bool load_config(t_io* io);

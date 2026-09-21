#pragma once

#include <stdbool.h>

#include "utils.h"

/** @brief Handles a STDIN request: reads a line from the terminal and replies
 * with it. @return false on failure. */
bool run_stdin(t_io* io);

/** @brief Handles a STDOUT request: prints the received buffer and acks.
 * @return false on failure. */
bool run_stdout(t_io* io);

/** @brief Handles a SLEEP request: blocks for the requested time and acks.
 * @return false on failure. */
bool run_sleep(t_io* io);

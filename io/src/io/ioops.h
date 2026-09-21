#pragma once

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/syscalls.h"

/** @brief Handles a STDIN request: reads a line from the terminal and replies
 * with it. @return false on failure. */
bool run_stdin(t_io* io);

/** @brief Handles a STDOUT request: prints the received buffer and acks.
 * @return false on failure. */
bool run_stdout(t_io* io);

/** @brief Handles a SLEEP request: blocks for the requested time and acks.
 * @return false on failure. */
bool run_sleep(t_io* io);

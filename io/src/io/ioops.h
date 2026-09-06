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

#include "utils.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registers_cpu.h"
#include "utils/syscalls.h"

bool run_stdin(t_io* io);
bool run_stdout(t_io* io);
bool run_sleep(t_io* io);

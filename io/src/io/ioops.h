#ifndef IO_IOOPS_H_
#define IO_IOOPS_H_

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
#include "utils/kernel_scheduler_cpu.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/registros.h"

bool io_tipo_stdin(t_modulo_io* sio);
bool io_tipo_stdout(t_modulo_io* sio);
bool io_tipo_sleep(t_modulo_io* sio);

#endif  // IO_IOOPS_H_
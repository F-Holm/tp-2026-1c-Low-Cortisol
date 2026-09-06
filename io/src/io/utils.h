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
#include "utils/io.h"
#include "utils/log.h"
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* port;
  int socket_io;
  int io_type;
  int sleep;
} t_io;

void close_io(t_io* io);
bool connect_to_scheduler(t_io* io);
bool parse_args(int argc, char** argv, t_io* io);
bool load_config(t_io* io);

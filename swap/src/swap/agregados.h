#ifndef SWAP_SWAP_H
#define SWAP_SWAP_H

#include <assert.h>
#include <commons/collections/list.h>
#include <commons/config.h>
#include <commons/log.h>
#include <netdb.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void cerrar_todo(t_log* logger, t_config* config, int socket_io);

#endif  // SWAP_SWAP_H
#ifndef IO_UTILS_H
#define IO_UTILS_H

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

#include "utils/client.h"
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
  char* puerto;
  int socket_io;
  int tipo_io;
  int sleep;
} t_modulo_io;

void cerrar_todo(t_modulo_io* modulo_io);
bool iniciar_enviar_tipo_io(t_modulo_io* modulo_io);
bool args(int argc, char** argv, t_modulo_io* modulo_io);
bool cargar_configs(t_modulo_io* modulo_io);

#endif  // IO_UTILS_H

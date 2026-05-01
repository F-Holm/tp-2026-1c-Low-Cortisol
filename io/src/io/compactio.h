#ifndef IO_COMPACTIO_H
#define IO_COMPACTIO_H

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

#include "utils/client.h"
#include "utils/hello.h"
#include "utils/io.h"
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* puerto;
  int socket_io;
  int tipo_io;
} t_modulo_io;

void cerrar_todo(t_modulo_io* modulo_io);
bool iniciar_enviar_tipo_io(t_modulo_io* modulo_io);
bool args(int argc, char** argv, t_modulo_io* modulo_io);
bool cargar_configs(t_modulo_io* modulo_io);

#endif  // IO_COMPACTIO_H

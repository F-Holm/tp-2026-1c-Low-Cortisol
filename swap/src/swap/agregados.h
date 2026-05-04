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

#include "utils/client.h"
#include "utils/hello.h"
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* puerto;
  int socket_swap;
} t_modulo_swap;

void cerrar_todo(t_modulo_swap* modulo_swap);
bool iniciar_conexion(t_modulo_swap* modulo_swap);
bool cargar_configs(t_modulo_swap* modulo_swap);
bool Args(int argc, char** argv, t_modulo_swap* modulo_swap);

#endif  // SWAP_SWAP_H
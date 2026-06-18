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
#include "utils/msg.h"

typedef struct
{
  t_config* config;
  t_log* logger;
  char* ip;
  char* puerto;
  int socket_swap;
  int swap_size;
  int block_size;
} t_modulo_swap;

typedef struct
{
  int swap_size;
  int block_size;
} t_envio_a_km;

void cerrar_todo(t_modulo_swap* modulo_swap);
bool iniciar_conexion(t_modulo_swap* modulo_swap);
bool inicializar_configuracion(t_modulo_swap* modulo_swap);

#endif  // SWAP_SWAP_H
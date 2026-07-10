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
#include "utils/logger.h"
#include "utils/msg.h"

typedef struct
{
  t_logger* logger;
  char* ip;
  char* puerto;
  int socket_swap;
  int tamanio_swap;
  int tamanio_bloque;
  char* swap_file_path;
  FILE* archivo_swap;
} t_modulo_swap;

typedef struct
{
  int swap_size;
  int block_size;
} t_envio_a_km;

void cerrar_todo(t_modulo_swap* modulo_swap, t_config* config);
bool iniciar_conexion(t_modulo_swap* modulo_swap, t_config* config);
bool inicializar_configuracion(t_modulo_swap* modulo_swap, t_config* config);
void escribir_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                     char* contenido_a_escribir);
void leer_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                 char* contenido_leido);

#endif  // SWAP_SWAP_H
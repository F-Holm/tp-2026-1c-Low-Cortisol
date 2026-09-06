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
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/swap_km.h"

typedef struct
{
  t_log* logger;
  char* ip;
  char* puerto;
  int socket_swap;
  int tamanio_swap;
  int tamanio_bloque;
  char* swap_file_path;
  FILE* archivo_swap;
} t_modulo_swap;

void cerrar_todo(t_modulo_swap* modulo_swap, t_config* config);
bool iniciar_conexion(t_modulo_swap* modulo_swap, t_config* config);
bool inicializar_configuracion(t_modulo_swap* modulo_swap, t_config* config);
void escribir_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                     char* contenido_a_escribir);
void leer_bloque(FILE* archivo_swap, int num_bloque, int tamanio_bloque,
                 char* contenido_leido);

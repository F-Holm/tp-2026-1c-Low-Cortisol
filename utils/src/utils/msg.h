#ifndef UTILS_MSG_
#define UTILS_MSG_

#include <assert.h>
#include <commons/collections/list.h>
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

typedef enum
{
  OP_CODE_ERROR,
  HANDSHAKE,
  MENSAJE,
  PAQUETE,
  PUERTO,
  IP
} op_code;

typedef struct
{
  int size;
  void* stream;
} t_buffer;

typedef struct
{
  op_code codigo_operacion;
  t_buffer* buffer;
} t_paquete;

typedef enum
{
  KERNEL_SCHEDULER,
  KERNEL_MEMORY,
  CPU,
  MEMORY_STICK,
  SWAP,
  IO,
  MODULE_ID_ERROR
} module_id;

const char* const HANDSHAKE_MSG[] = {
    "kernel_scheduler",
    "kernel_memory",
    "cpu",
    "memory_stick",
    "swap",
    "io"
}

int recibir_operacion(int);
void* recibir_buffer(int*, int);

// Handshake
void enviar_handshake(module_id mi_modulo_id, int socket);
module_id recibir_handshake(int socket);

// Mensajes
void enviar_mensaje(char* mensaje, int socket_cliente);
char* recibir_mensaje(int);

// Paquetes
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
t_list* recibir_paquete(int);
void eliminar_paquete(t_paquete* paquete);

#endif /* UTILS_MSG_ */

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

int recibir_operacion(int socket);
void* recibir_buffer(int* size, int socket);
void enviar_string(op_code codigo_operacion, char* mensaje, int socket);
char* recibir_string(int socket);
module_id handshake_msg_to_module_id(char* handshake_msg);
t_paquete* crear_paquete(void);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket);
t_list* recibir_paquete(int);
void eliminar_paquete(t_paquete* paquete);

#endif /* UTILS_MSG_ */

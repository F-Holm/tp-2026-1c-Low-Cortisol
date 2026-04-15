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

const char* const HANDSHAKE_MSG[] = {"kernel_scheduler", "kernel_memory", "cpu",
                                     "memory_stick",     "swap",          "io"}

/**
 * @brief Recibe el código de operación
 * @param socket
 * @return Código de operación (enum / int)
 * @note Usar siempre antes de llamar a a una función de recibir o leer algo del
 * buffer
 */
int recibir_operacion(int socket);

/**
 * @brief Recibe datos del buffer
 * @param size Cantidad de bytes que se quieren leer
 * @param socket
 * @return Puntero al buffer
 * @note Usar después de recibir_operacion
 * @note Liberar memoria dinámica del buffer retornado
 */
void* recibir_buffer(int* size, int socket);

/**
 * @brief Envia un char*
 * @param codigo_operacion Código de operación (enum / int)
 * @param mensaje
 * @param socket
 * @return No devuelve nada
 */
void enviar_string(int codigo_operacion, char* mensaje, int socket);

/**
 * @brief Recibe un char*
 * @param socket
 * @return Puntero al buffer
 * @note Usar después de recibir_operacion
 * @note Liberar memoria dinámica del buffer retornado
 */
char* recibir_string(int socket);

/**
 * @brief Devuelve el id del módulo, este id es de tipo del enum module_id
 * @param handshake_msg char* de HANDSHAKE_MSG
 * @return No devuelve nada
 * @note Usar después de recibir_string}
 */
int handshake_msg_to_module_id(char* handshake_msg);

t_paquete* crear_paquete(void);

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

void enviar_paquete(t_paquete* paquete, int socket);

/**
 * @note Usar después de recibir_operacion
+ */
t_list* recibir_paquete(int);

void eliminar_paquete(t_paquete* paquete);

#endif /* UTILS_MSG_ */

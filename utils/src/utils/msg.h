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
  OP_HANDSHAKE,
  OP_MENSAJE,
  OP_PAQUETE,
  OP_PUERTO,
  OP_IP,
  OP_ID_CPU,
  OP_INFO_SWAP,
  OP_TAMANIO_MEMORIA,
  OP_TIPO_IO,
  OP_PETICION_IO_STDIN,
  OP_PETICION_IO_STDOUT,
  OP_PETICION_IO_SLEEP,
  OP_RESPUESTA_STDIN,
  OP_RESPUESTA_STDOUT,
  OP_RESPUESTA_SLEEP,
  OP_MEMORIA_CORRUPTA,
  OP_CICLO_CPU_OK,
  OP_SYSCALL_MUTEX_CREATE,  // No cambiar el orden de las syscalls
  OP_SYSCALL_MUTEX_LOCK,    // No poner elementos entre las syscalls
  OP_SYSCALL_MUTEX_UNLOCK,
  OP_SYSCALL_MEM_ALLOC,
  OP_SYSCALL_MEM_FREE,
  OP_SYSCALL_SLEEP,
  OP_SYSCALL_STDOUT,
  OP_SYSCALL_STDIN,
  OP_SYSCALL_INIT_PROC,
  OP_SYSCALL_EXIT,
  OP_CONTINUAR_PROCESO,
  OP_NUEVO_PROCESO,     // No responder
  OP_TERMINAR_PROCESO,  // No responder
  OP_TAMANIO_TOTAL_MEMORIA,
  OP_SIGUIENTE_INSTRUCCION,
  OP_ENVIAR_INSTRUCCION,
  OP_OK,
  OP_PEDIR_CONTEXTO,
  OP_ENVIAR_CONTEXTO,
  OP_CONTEXTO_ACTUALIZADO,
  OP_SIN_INTERRUPCION,
  OP_INTERRUPCION,
  OP_MEMORY_STICK_LEER,
  OP_MEMORY_STICK_ESCRIBIR,
  OP_MEMORY_STICK_LEIDO,
  OP_MEMORY_STICK_ESCRITO
} t_op_code;

typedef struct
{
  int size;
  void* stream;
} t_buffer;

typedef struct
{
  t_op_code codigo_operacion;
  t_buffer* buffer;
} t_paquete;

typedef enum
{
  MID_KERNEL_SCHEDULER,
  MID_KERNEL_MEMORY,
  MID_CPU,
  MID_MEMORY_STICK,
  MID_SWAP,
  MID_IO,
  MID_MODULE_ID_ERROR
} t_module_id;

extern const char* const HANDSHAKE_MSG[6];

/**
 * @brief Recibe el código de operación
 * @param socket_fd
 * @return Código de operación (enum / int)
 * @note Usar siempre antes de llamar a a una función de recibir o leer algo del
         buffer
 */
int recibir_operacion(int socket_fd);

/**
 * @brief Recibe datos del buffer
 * @param size Cantidad de bytes que se quieren leer
 * @param socket_fd
 * @return Puntero al buffer
 * @note Usar después de recibir_operacion()
 * @note Liberar memoria dinámica del buffer retornado
 */
void* recibir_buffer(int* size, int socket_fd);

/**
 * @brief Envia un void*
 * @param codigo_operacion Código de operación (enum / int)
 * @param buffer
 * @param size
 * @param socket_fd
 * @return Devuelve un bool: false = envio nulo o receptor desconectado
 */
bool enviar_buffer(int codigo_operacion, void* buffer, int size, int socket_fd);

/**
 * @brief Envia un char*
 * @param codigo_operacion Código de operación (enum / int)
 * @param mensaje
 * @param socket_fd
 * @return Devuelve un bool: false = envio nulo o receptor desconectado
 */
bool enviar_string(int codigo_operacion, char* mensaje, int socket_fd);

/**
 * @brief Recibe un char*
 * @param socket_fd
 * @return Puntero al buffer
 * @note Usar después de recibir_operacion()
 * @note Liberar memoria dinámica del buffer retornado
 */
char* recibir_string(int socket_fd);

/**
 * @brief Devuelve el id del módulo, este id es de tipo t_module_id (se puede
 * usar cualquier entero)
 * @param handshake_msg char* de HANDSHAKE_MSG[]
 * @return Retorna un int que representa el id_module que es un enum (se puede
           castear)
 * @note Usar después de recibir_string()
 */
int handshake_msg_to_module_id(char* handshake_msg);

/**
 * @brief Envia handshake
 * @param id_modulo entero de tipo t_module_id (se puede usar cualquier entero)
 * @param socket_fd
 * @return Devuelve un bool: false = envio nulo o receptor desconectado
 */
bool enviar_handshake(int id_modulo, int socket_fd);

/**
 * @brief Recibe handshake
 * @param socket_fd
 * @return Retorna un int que representa el id_module que es un enum (se puede
           castear)
 */
int recibir_handshake(int socket_fd);

/**
 * @brief Crea un paquete
 * @param codigo_operacion código de operación del paquete
 * @return Devuelve un t_paqute* inicializado
 * @note Llamar a eliminar_paquete() para liberar la memoria reservada en esta
         función
 */
t_paquete* crear_paquete(int codigo_operacion);

/**
 * @brief Agrega el elemento al paquete
 * @param paquete
 * @param valor
 * @param tamanio
 * @return No devuelve nada
 * @note Usar después de crear_paquete()
 */
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);

/**
 * @brief Agrega el elemento al paquete
 * @param paquete
 * @param valor
 * @return No devuelve nada
 * @note Usar después de crear_paquete()
 */
void agregar_string_a_paquete(t_paquete* paquete, char* valor);

/**
 * @brief Envia el paquete
 * @param socket_fd
 * @return Devuelve un bool: false = envio nulo o receptor desconectado
 * @note Usar después de crear_paquete()
 */
bool enviar_paquete(t_paquete* paquete, int socket_fd);

/**
 * @brief Recibe un paquete y lo guarda en una lista
 * @param socket_fd
 * @return Devuelve una lista de void* con el contenido de cada elemento dentro
           del paquete
 * @note La lista retornada debe ser liberada después de su uso.
 * @note Hay que liberar cada elemento de la lista luego de su uso.
 * @note Los strings agregador mediante agregar_string_a_paquete ya tienen '\0'
 */
t_list* recibir_paquete(int socket_fd);

/**
 * @brief Elimina el paquete
 * @param paquete
 * @return No devuelve nada
 * @note Usar después de crear_paquete()
 */
void eliminar_paquete(t_paquete* paquete);

#endif /* UTILS_MSG_ */

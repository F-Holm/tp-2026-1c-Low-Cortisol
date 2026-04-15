#include "utils/msg.h"

int recibir_operacion(int socket_cliente)
{
  int cod_op;
  if (recv(socket_cliente, &cod_op, sizeof(int), MSG_WAITALL) > 0)
    return cod_op;
  else
  {
    return -1;
  }
}

void crear_buffer(t_paquete* paquete)
{
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = 0;
  paquete->buffer->stream = NULL;
}

void* recibir_buffer(int* size, int socket_cliente)
{
  void* buffer;

  recv(socket_cliente, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size);
  recv(socket_cliente, buffer, *size, MSG_WAITALL);

  return buffer;
}

module_id handshake_msg_to_module_id(char* handshake_msg)
{
  int total_modulos = 6;

  for (int i = 0; i < total_modulos; i++)
    if (strcmp(handshake_msg, HANDSHAKE_MSG[i]) == 0)
      return (module_id)i;
  return MODULE_ID_ERROR;
}

void enviar_handshake(module_id mi_modulo_id, int socket)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));
  char* msg = HANDSHAKE_MSG[mi_modulo_id];

  paquete->codigo_operacion = HANDSHAKE;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(msg) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, msg, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);

  void* a_enviar = serializar_paquete(paquete, bytes);

  send(socket, a_enviar, bytes, 0);

  free(a_enviar);
  eliminar_paquete(paquete);
}

module_id recibir_handshake(int socket)
{
  int size;
  module_id module;
  char* buffer = recibir_buffer(&size, socket_cliente);
  module = handshake_msg_to_module_id(buffer);
  free(buffer);
  return module;
}

void enviar_mensaje(char* mensaje, int socket)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));

  paquete->codigo_operacion = MENSAJE;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(mensaje) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);

  void* a_enviar = serializar_paquete(paquete, bytes);

  send(socket, a_enviar, bytes, 0);

  free(a_enviar);
  eliminar_paquete(paquete);
}

// acordarse de liberar memoria dinamica del puntero retornado
char* recibir_mensaje(int socket_cliente)
{
  int size;
  return recibir_buffer(&size, socket_cliente);
}

void* serializar_paquete(t_paquete* paquete, int bytes)
{
  void* magic = malloc(bytes);
  int desplazamiento = 0;

  memcpy(magic + desplazamiento, &(paquete->codigo_operacion), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, &(paquete->buffer->size), sizeof(int));
  desplazamiento += sizeof(int);
  memcpy(magic + desplazamiento, paquete->buffer->stream,
         paquete->buffer->size);
  desplazamiento += paquete->buffer->size;

  return magic;
}

t_paquete* crear_paquete(void)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));
  paquete->codigo_operacion = PAQUETE;
  crear_buffer(paquete);
  return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio)
{
  paquete->buffer->stream = realloc(
      paquete->buffer->stream, paquete->buffer->size + tamanio + sizeof(int));

  memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio,
         sizeof(int));
  memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), valor,
         tamanio);

  paquete->buffer->size += tamanio + sizeof(int);
}

void enviar_paquete(t_paquete* paquete, int socket)
{
  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void* a_enviar = serializar_paquete(paquete, bytes);

  send(socket, a_enviar, bytes, 0);

  free(a_enviar);
}

t_list* recibir_paquete(int socket_cliente)
{
  int size;
  int desplazamiento = 0;
  void* buffer;
  t_list* valores = list_create();
  int tamanio;

  buffer = recibir_buffer(&size, socket_cliente);
  while (desplazamiento < size)
  {
    memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    char* valor = malloc(tamanio + 1);
    memcpy(valor, buffer + desplazamiento, tamanio);
    valor[tamanio] = '\0';
    desplazamiento += tamanio;
    list_add(valores, valor);
  }
  free(buffer);
  return valores;
}

void eliminar_paquete(t_paquete* paquete)
{
  free(paquete->buffer->stream);
  free(paquete->buffer);
  free(paquete);
}

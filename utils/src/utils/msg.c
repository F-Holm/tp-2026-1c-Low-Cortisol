#include "utils/msg.h"

int recibir_operacion(int socket)
{
  int cod_op;
  if (recv(socket, &cod_op, sizeof(int), MSG_WAITALL) > 0)
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

void* recibir_buffer(int* size, int socket)
{
  void* buffer;

  recv(socket, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size);
  recv(socket, buffer, *size, MSG_WAITALL);

  return buffer;
}

// String
void enviar_string(op_code codigo_operacion, char* mensaje, int socket)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));

  paquete->codigo_operacion = codigo_operacion;
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

char* recibir_string(int socket)
{
  int size;
  return recibir_buffer(&size, socket);
}

// Handshake
module_id handshake_msg_to_module_id(char* handshake_msg)
{
  int total_modulos = 6;

  for (int i = 0; i < total_modulos; i++)
    if (strcmp(handshake_msg, HANDSHAKE_MSG[i]) == 0)
      return (module_id)i;
  return MODULE_ID_ERROR;
}

// Paquete
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

t_list* recibir_paquete(int socket)
{
  int size;
  int desplazamiento = 0;
  void* buffer;
  t_list* valores = list_create();
  int tamanio;

  buffer = recibir_buffer(&size, socket);
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

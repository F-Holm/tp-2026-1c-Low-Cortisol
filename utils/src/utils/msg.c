#include "utils/msg.h"

const char* const HANDSHAKE_MSG[] = {"kernel_scheduler", "kernel_memory", "cpu",
                                     "memory_stick",     "swap",          "io"};

int recibir_operacion(int socket_fd)
{
  int cod_op;
  if (recv(socket_fd, &cod_op, sizeof(int), MSG_WAITALL) > 0)
    return cod_op;
  else
  {
    return OP_CODE_ERROR;
  }
}

void crear_buffer(t_paquete* paquete)
{
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = 0;
  paquete->buffer->stream = NULL;
}

void* recibir_buffer(int* size, int socket_fd)
{
  void* buffer;

  recv(socket_fd, size, sizeof(int), MSG_WAITALL);
  buffer = malloc(*size);
  recv(socket_fd, buffer, *size, MSG_WAITALL);

  return buffer;
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

bool enviar_buffer(int codigo_operacion, void* buffer, int size, int socket_fd)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));

  paquete->codigo_operacion = codigo_operacion;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = size;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, buffer, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);

  void* a_enviar = serializar_paquete(paquete, bytes);

  bool ret = send(socket_fd, a_enviar, bytes, MSG_NOSIGNAL) > 0;

  free(a_enviar);
  eliminar_paquete(paquete);

  return ret;
}

// String
bool enviar_string(int codigo_operacion, char* mensaje, int socket_fd)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));

  paquete->codigo_operacion = codigo_operacion;
  paquete->buffer = malloc(sizeof(t_buffer));
  paquete->buffer->size = strlen(mensaje) + 1;
  paquete->buffer->stream = malloc(paquete->buffer->size);
  memcpy(paquete->buffer->stream, mensaje, paquete->buffer->size);

  int bytes = paquete->buffer->size + 2 * sizeof(int);

  void* a_enviar = serializar_paquete(paquete, bytes);

  bool ret = send(socket_fd, a_enviar, bytes, MSG_NOSIGNAL) > 0;

  free(a_enviar);
  eliminar_paquete(paquete);

  return ret;
}

char* recibir_string(int socket_fd)
{
  int size;
  return recibir_buffer(&size, socket_fd);
}

// Handshake
int handshake_msg_to_module_id(char* handshake_msg)
{
  int total_modulos = 6;

  for (int i = 0; i < total_modulos; i++)
    if (strcmp(handshake_msg, HANDSHAKE_MSG[i]) == 0)
      return i;
  return MID_MODULE_ID_ERROR;
}

bool enviar_handshake(int id_modulo, int socket_fd)
{
  return enviar_string(OP_HANDSHAKE, (char*)HANDSHAKE_MSG[id_modulo],
                       socket_fd);
}

int recibir_handshake(int socket_fd)
{
  if (recibir_operacion(socket_fd) != OP_HANDSHAKE)
    return MID_MODULE_ID_ERROR;
  char* msg = recibir_string(socket_fd);
  int id_module = handshake_msg_to_module_id(msg);
  free(msg);
  return id_module;
}

// Paquete
t_paquete* crear_paquete(int codigo_operacion)
{
  t_paquete* paquete = malloc(sizeof(t_paquete));
  paquete->codigo_operacion = codigo_operacion;
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

void agregar_string_a_paquete(t_paquete* paquete, char* valor)
{
  agregar_a_paquete(paquete, valor, strlen(valor) + 1);
}

bool enviar_paquete(t_paquete* paquete, int socket_fd)
{
  int bytes = paquete->buffer->size + 2 * sizeof(int);
  void* a_enviar = serializar_paquete(paquete, bytes);

  bool ret = send(socket_fd, a_enviar, bytes, MSG_NOSIGNAL) > 0;

  free(a_enviar);

  return ret;
}

t_list* recibir_paquete(int socket_fd)
{
  int size;
  int desplazamiento = 0;
  void* buffer;
  t_list* valores = list_create();
  int tamanio;

  buffer = recibir_buffer(&size, socket_fd);
  while (desplazamiento < size)
  {
    memcpy(&tamanio, buffer + desplazamiento, sizeof(int));
    desplazamiento += sizeof(int);
    char* valor = malloc(tamanio);
    memcpy(valor, buffer + desplazamiento, tamanio);
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

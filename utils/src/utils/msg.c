#include "utils/msg.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/collections/list.h"
#include "utils/sockets.h"

const char* const HANDSHAKE_MSG[] = {"kernel_scheduler", "kernel_memory", "cpu",
                                     "memory_stick",     "swap",          "io"};

static void create_buffer(t_packet* packet);
static void* serialize_packet(t_packet* packet, int bytes);

int receive_op_code(t_socket* socket)
{
  int op_code;
  if (socket_receive(socket, &op_code, sizeof(int)))
    return op_code;
  else
  {
    return OP_CODE_ERROR;
  }
}

void* receive_buffer(int* size, t_socket* socket)
{
  if (!socket_receive(socket, size, sizeof(int)))
  {
    *size = 0;
    return NULL;
  }
  if (*size == 0)
  {
    return NULL;
  }

  void* buffer = malloc(*size);
  socket_receive(socket, buffer, *size);

  return buffer;
}

bool send_buffer(int op_code, void* buffer, int size, t_socket* socket)
{
  t_packet* packet = malloc(sizeof(t_packet));

  packet->op_code = op_code;
  packet->buffer = malloc(sizeof(t_buffer));
  packet->buffer->size = size;
  packet->buffer->stream = malloc(packet->buffer->size);
  memcpy(packet->buffer->stream, buffer, packet->buffer->size);

  int bytes = packet->buffer->size + 2 * sizeof(int);

  void* to_send = serialize_packet(packet, bytes);

  bool ret = socket_send(socket, to_send, bytes);

  free(to_send);
  destroy_packet(packet);

  return ret;
}

bool send_string(int op_code, char* message, t_socket* socket)
{
  t_packet* packet = malloc(sizeof(t_packet));

  packet->op_code = op_code;
  packet->buffer = malloc(sizeof(t_buffer));
  packet->buffer->size = strlen(message) + 1;
  packet->buffer->stream = malloc(packet->buffer->size);
  memcpy(packet->buffer->stream, message, packet->buffer->size);

  int bytes = packet->buffer->size + 2 * sizeof(int);

  void* to_send = serialize_packet(packet, bytes);

  bool ret = socket_send(socket, to_send, bytes);

  free(to_send);
  destroy_packet(packet);

  return ret;
}

char* receive_string(t_socket* socket)
{
  int size;
  return receive_buffer(&size, socket);
}

int handshake_msg_to_module_id(char* handshake_msg)
{
  int module_count = 6;

  for (int i = 0; i < module_count; i++)
    if (strcmp(handshake_msg, HANDSHAKE_MSG[i]) == 0)
      return i;
  return MID_MODULE_ID_ERROR;
}

bool send_handshake(int module_id, t_socket* socket)
{
  return send_string(OP_HANDSHAKE, (char*)HANDSHAKE_MSG[module_id], socket);
}

int receive_handshake(t_socket* socket)
{
  if (receive_op_code(socket) != OP_HANDSHAKE)
    return MID_MODULE_ID_ERROR;
  char* msg = receive_string(socket);
  int module_id = handshake_msg_to_module_id(msg);
  free(msg);
  return module_id;
}

t_packet* create_packet(int op_code)
{
  t_packet* packet = malloc(sizeof(t_packet));
  packet->op_code = op_code;
  create_buffer(packet);
  return packet;
}

void packet_append(t_packet* packet, void* value, int size)
{
  packet->buffer->stream = realloc(packet->buffer->stream,
                                   packet->buffer->size + size + sizeof(int));

  memcpy(packet->buffer->stream + packet->buffer->size, &size, sizeof(int));
  memcpy(packet->buffer->stream + packet->buffer->size + sizeof(int), value,
         size);

  packet->buffer->size += size + sizeof(int);
}

void packet_append_string(t_packet* packet, char* value)
{
  packet_append(packet, value, strlen(value) + 1);
}

bool send_packet(t_packet* packet, t_socket* socket)
{
  int bytes = packet->buffer->size + 2 * sizeof(int);
  void* to_send = serialize_packet(packet, bytes);

  bool ret = socket_send(socket, to_send, bytes);

  free(to_send);

  return ret;
}

t_list* receive_packet(t_socket* socket)
{
  int size;
  int offset = 0;
  void* buffer;
  t_list* values = list_create();
  int element_size;

  buffer = receive_buffer(&size, socket);
  while (offset < size)
  {
    memcpy(&element_size, buffer + offset, sizeof(int));
    offset += sizeof(int);
    if (element_size == 0)
    {
      break;
    }
    char* value = malloc(element_size);
    memcpy(value, buffer + offset, element_size);
    offset += element_size;
    list_add(values, value);
  }
  if (buffer != NULL)
  {
    free(buffer);
  }
  return values;
}

void destroy_packet(t_packet* packet)
{
  free(packet->buffer->stream);
  free(packet->buffer);
  free(packet);
}

static void create_buffer(t_packet* packet)
{
  packet->buffer = malloc(sizeof(t_buffer));
  packet->buffer->size = 0;
  packet->buffer->stream = NULL;
}

static void* serialize_packet(t_packet* packet, int bytes)
{
  void* magic = malloc(bytes);
  int offset = 0;

  memcpy(magic + offset, &(packet->op_code), sizeof(int));
  offset += sizeof(int);
  memcpy(magic + offset, &(packet->buffer->size), sizeof(int));
  offset += sizeof(int);
  if (packet->buffer->stream != NULL)
  {
    memcpy(magic + offset, packet->buffer->stream, packet->buffer->size);
  }
  offset += packet->buffer->size;

  return magic;
}

#include "utils/msg.h"

const char* const HANDSHAKE_MSG[] = {"kernel_scheduler", "kernel_memory", "cpu",
                                     "memory_stick",     "swap",          "io"};

static void create_buffer(t_packet* packet);
static void* serialize_packet(t_packet* packet, int bytes);

int create_connection(char* ip, char* port)
{
  struct addrinfo hints;
  struct addrinfo* server_info;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  if (getaddrinfo(ip, port, &hints, &server_info) != 0)
    return -1;

  int fd_socket = socket(server_info->ai_family, server_info->ai_socktype,
                         server_info->ai_protocol);
  if (fd_socket == -1)
  {
    freeaddrinfo(server_info);
    return -1;
  }

  if (connect(fd_socket, server_info->ai_addr, server_info->ai_addrlen) == -1)
  {
    freeaddrinfo(server_info);
    close(fd_socket);
    return -1;
  }

  freeaddrinfo(server_info);

  return fd_socket;
}

int start_server(char* port)
{
  struct addrinfo hints, *servinfo;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  getaddrinfo(NULL, port, &hints, &servinfo);

  int server_socket =
      socket(hints.ai_family, hints.ai_socktype, hints.ai_protocol);

  // Make the socket reusable; remove if it causes issues.
  int opt = 1;
  setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  bind(server_socket, servinfo->ai_addr, servinfo->ai_addrlen);
  listen(server_socket, SOMAXCONN);

  freeaddrinfo(servinfo);

  return server_socket;
}

int receive_op_code(int socket_fd)
{
  int op_code;
  if (recv(socket_fd, &op_code, sizeof(int), MSG_WAITALL) > 0)
    return op_code;
  else
  {
    return OP_CODE_ERROR;
  }
}

void* receive_buffer(int* size, int socket_fd)
{
  recv(socket_fd, size, sizeof(int), MSG_WAITALL);
  if (*size == 0)
  {
    return NULL;
  }

  void* buffer = malloc(*size);
  recv(socket_fd, buffer, *size, MSG_WAITALL);

  return buffer;
}

bool send_buffer(int op_code, void* buffer, int size, int socket_fd)
{
  t_packet* packet = malloc(sizeof(t_packet));

  packet->op_code = op_code;
  packet->buffer = malloc(sizeof(t_buffer));
  packet->buffer->size = size;
  packet->buffer->stream = malloc(packet->buffer->size);
  memcpy(packet->buffer->stream, buffer, packet->buffer->size);

  int bytes = packet->buffer->size + 2 * sizeof(int);

  void* to_send = serialize_packet(packet, bytes);

  bool ret = send(socket_fd, to_send, bytes, MSG_NOSIGNAL) > 0;

  free(to_send);
  destroy_packet(packet);

  return ret;
}

bool send_string(int op_code, char* message, int socket_fd)
{
  t_packet* packet = malloc(sizeof(t_packet));

  packet->op_code = op_code;
  packet->buffer = malloc(sizeof(t_buffer));
  packet->buffer->size = strlen(message) + 1;
  packet->buffer->stream = malloc(packet->buffer->size);
  memcpy(packet->buffer->stream, message, packet->buffer->size);

  int bytes = packet->buffer->size + 2 * sizeof(int);

  void* to_send = serialize_packet(packet, bytes);

  bool ret = send(socket_fd, to_send, bytes, MSG_NOSIGNAL) > 0;

  free(to_send);
  destroy_packet(packet);

  return ret;
}

char* receive_string(int socket_fd)
{
  int size;
  return receive_buffer(&size, socket_fd);
}

int handshake_msg_to_module_id(char* handshake_msg)
{
  int module_count = 6;

  for (int i = 0; i < module_count; i++)
    if (strcmp(handshake_msg, HANDSHAKE_MSG[i]) == 0)
      return i;
  return MID_MODULE_ID_ERROR;
}

bool send_handshake(int module_id, int socket_fd)
{
  return send_string(OP_HANDSHAKE, (char*)HANDSHAKE_MSG[module_id], socket_fd);
}

int receive_handshake(int socket_fd)
{
  if (receive_op_code(socket_fd) != OP_HANDSHAKE)
    return MID_MODULE_ID_ERROR;
  char* msg = receive_string(socket_fd);
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

bool send_packet(t_packet* packet, int socket_fd)
{
  int bytes = packet->buffer->size + 2 * sizeof(int);
  void* to_send = serialize_packet(packet, bytes);

  bool ret = send(socket_fd, to_send, bytes, MSG_NOSIGNAL) > 0;

  free(to_send);

  return ret;
}

t_list* receive_packet(int socket_fd)
{
  int size;
  int offset = 0;
  void* buffer;
  t_list* values = list_create();
  int element_size;

  buffer = receive_buffer(&size, socket_fd);
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

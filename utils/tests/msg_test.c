#include "utils/msg.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/collections/list.h"
#include "utils/sockets.h"

/* Every test that touches a socket gets a hard ceiling so a protocol bug shows
 * up as a failure instead of a hang. */
TestSuite(msg, .timeout = 5.0);

/* Opens a loopback connection on an ephemeral port. Returns the client socket
 * and, through @p server_out, the accepted server socket. Both must be
 * destroyed by the caller. */
static t_socket* connected_pair(t_socket** server_out)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, NULL, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener, "socket_create(SERVER) failed");

  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));

  t_socket* client =
      socket_create(SOCKET_KIND_CLIENT, "127.0.0.1", port, false);
  cr_assert_not_null(client, "socket_create(CLIENT) failed");

  t_socket* server = socket_accept(listener, false);
  cr_assert_not_null(server, "socket_accept failed");

  socket_destroy(listener);
  *server_out = server;
  return client;
}

Test(msg, send_buffer_roundtrips_the_op_code_and_the_payload)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  int payload[3] = {111, 222, 333};
  cr_assert(send_buffer(OP_NEW_PROCESS, payload, sizeof(payload), client));

  cr_assert_eq(receive_op_code(server), OP_NEW_PROCESS);
  int size = 0;
  void* received = receive_buffer(&size, server);
  cr_assert_eq(size, (int)sizeof(payload));
  cr_assert_eq(memcmp(received, payload, size), 0);

  free(received);
  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, send_string_roundtrips)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  cr_assert(send_string(OP_INTERRUPT, "preempted", client));

  cr_assert_eq(receive_op_code(server), OP_INTERRUPT);
  char* received = receive_string(server);
  cr_assert_str_eq(received, "preempted");

  free(received);
  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, send_string_carries_the_empty_string)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  cr_assert(send_string(OP_STDOUT_RESPONSE, "", client));

  cr_assert_eq(receive_op_code(server), OP_STDOUT_RESPONSE);
  char* received = receive_string(server);
  cr_assert_str_eq(received, "");

  free(received);
  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, receive_op_code_on_a_closed_peer_is_op_code_error)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  socket_destroy(client);
  cr_assert_eq(receive_op_code(server), OP_CODE_ERROR);

  socket_destroy(server);
}

Test(msg, send_helpers_return_false_on_an_invalid_socket)
{
  int value = 0;
  cr_assert_not(send_buffer(OP_HANDSHAKE, &value, sizeof(value), NULL));
  cr_assert_not(send_string(OP_HANDSHAKE, "x", NULL));

  t_packet* packet = create_packet(OP_HANDSHAKE);
  packet_append_string(packet, "x");
  cr_assert_not(send_packet(packet, NULL));
  destroy_packet(packet);
}

Test(msg, handshake_msg_to_module_id_maps_every_name)
{
  cr_assert_eq(handshake_msg_to_module_id("kernel_scheduler"),
               MID_KERNEL_SCHEDULER);
  cr_assert_eq(handshake_msg_to_module_id("kernel_memory"), MID_KERNEL_MEMORY);
  cr_assert_eq(handshake_msg_to_module_id("cpu"), MID_CPU);
  cr_assert_eq(handshake_msg_to_module_id("memory_stick"), MID_MEMORY_STICK);
  cr_assert_eq(handshake_msg_to_module_id("swap"), MID_SWAP);
  cr_assert_eq(handshake_msg_to_module_id("io"), MID_IO);
  cr_assert_eq(handshake_msg_to_module_id("something-else"),
               MID_MODULE_ID_ERROR);
}

Test(msg, handshake_roundtrips_the_module_id)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  cr_assert(send_handshake(MID_CPU, client));
  cr_assert_eq(receive_handshake(server), MID_CPU);

  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, receive_handshake_rejects_a_non_handshake_message)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  cr_assert(send_string(OP_ID_CPU, "cpu", client));
  cr_assert_eq(receive_handshake(server), MID_MODULE_ID_ERROR);

  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, packet_roundtrips_a_sequence_of_elements)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  t_packet* packet = create_packet(OP_PACKET);
  int number = 42;
  packet_append(packet, &number, sizeof(number));
  packet_append_string(packet, "hello");
  cr_assert(send_packet(packet, client));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(server), OP_PACKET);
  t_list* values = receive_packet(server);
  cr_assert_eq(list_size(values), 2);
  cr_assert_eq(*(int*)list_get(values, 0), 42);
  cr_assert_str_eq((char*)list_get(values, 1), "hello");

  list_destroy_and_destroy_elements(values, free);
  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, an_empty_packet_yields_an_empty_list)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  t_packet* packet = create_packet(OP_KERNEL_MEMORY_RUNNING);
  cr_assert(send_packet(packet, client));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(server), OP_KERNEL_MEMORY_RUNNING);
  t_list* values = receive_packet(server);
  cr_assert_eq(list_size(values), 0);

  list_destroy(values);
  socket_destroy(client);
  socket_destroy(server);
}

Test(msg, receive_buffer_reports_a_zero_length_payload_as_null)
{
  t_socket* server;
  t_socket* client = connected_pair(&server);

  int empty = 0;
  cr_assert(send_buffer(OP_MEMORY_FREED, &empty, 0, client));

  cr_assert_eq(receive_op_code(server), OP_MEMORY_FREED);
  int size = -1;
  void* received = receive_buffer(&size, server);
  cr_assert_eq(size, 0);
  cr_assert_null(received);

  socket_destroy(client);
  socket_destroy(server);
}

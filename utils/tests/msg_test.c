#include <arpa/inet.h>
#include <criterion/criterion.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "utils/collections/list.h"
#include "utils/msg.h"

/* Every test that touches a socket gets a hard ceiling so a protocol bug shows
 * up as a failure instead of a hang. */
TestSuite(msg, .timeout = 5.0);

/* Opens a loopback connection on an ephemeral port. Returns the client fd and,
 * through @p server_out, the accepted server fd. Both must be closed by the
 * caller. */
static int connected_pair(int* server_out)
{
  int listen_fd = start_server("0");
  cr_assert_geq(listen_fd, 0, "start_server failed");

  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  cr_assert_eq(getsockname(listen_fd, (struct sockaddr*)&address, &length), 0);

  char port[16];
  snprintf(port, sizeof(port), "%d", ntohs(address.sin_port));

  int client_fd = create_connection("127.0.0.1", port);
  cr_assert_geq(client_fd, 0, "create_connection failed");

  int server_fd = accept(listen_fd, NULL, NULL);
  cr_assert_geq(server_fd, 0, "accept failed");

  close(listen_fd);
  *server_out = server_fd;
  return client_fd;
}

Test(msg, create_connection_to_an_unbound_port_fails)
{
  int listen_fd = start_server("0");
  struct sockaddr_in address;
  socklen_t length = sizeof(address);
  getsockname(listen_fd, (struct sockaddr*)&address, &length);
  char port[16];
  snprintf(port, sizeof(port), "%d", ntohs(address.sin_port));
  close(listen_fd);

  cr_assert_eq(create_connection("127.0.0.1", port), -1);
}

Test(msg, create_connection_with_an_invalid_service_fails)
{
  cr_assert_eq(create_connection("127.0.0.1", "not-a-port"), -1);
}

Test(msg, send_buffer_roundtrips_the_op_code_and_the_payload)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  int payload[3] = {111, 222, 333};
  cr_assert(send_buffer(OP_NEW_PROCESS, payload, sizeof(payload), client_fd));

  cr_assert_eq(receive_op_code(server_fd), OP_NEW_PROCESS);
  int size = 0;
  void* received = receive_buffer(&size, server_fd);
  cr_assert_eq(size, (int)sizeof(payload));
  cr_assert_eq(memcmp(received, payload, size), 0);

  free(received);
  close(client_fd);
  close(server_fd);
}

Test(msg, send_string_roundtrips)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  cr_assert(send_string(OP_INTERRUPT, "preempted", client_fd));

  cr_assert_eq(receive_op_code(server_fd), OP_INTERRUPT);
  char* received = receive_string(server_fd);
  cr_assert_str_eq(received, "preempted");

  free(received);
  close(client_fd);
  close(server_fd);
}

Test(msg, send_string_carries_the_empty_string)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  cr_assert(send_string(OP_STDOUT_RESPONSE, "", client_fd));

  cr_assert_eq(receive_op_code(server_fd), OP_STDOUT_RESPONSE);
  char* received = receive_string(server_fd);
  cr_assert_str_eq(received, "");

  free(received);
  close(client_fd);
  close(server_fd);
}

Test(msg, receive_op_code_on_a_closed_peer_is_op_code_error)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  close(client_fd);
  cr_assert_eq(receive_op_code(server_fd), OP_CODE_ERROR);

  close(server_fd);
}

Test(msg, send_helpers_return_false_on_an_invalid_socket)
{
  int value = 0;
  cr_assert_not(send_buffer(OP_HANDSHAKE, &value, sizeof(value), -1));
  cr_assert_not(send_string(OP_HANDSHAKE, "x", -1));

  t_packet* packet = create_packet(OP_HANDSHAKE);
  packet_append_string(packet, "x");
  cr_assert_not(send_packet(packet, -1));
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
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  cr_assert(send_handshake(MID_CPU, client_fd));
  cr_assert_eq(receive_handshake(server_fd), MID_CPU);

  close(client_fd);
  close(server_fd);
}

Test(msg, receive_handshake_rejects_a_non_handshake_message)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  cr_assert(send_string(OP_ID_CPU, "cpu", client_fd));
  cr_assert_eq(receive_handshake(server_fd), MID_MODULE_ID_ERROR);

  close(client_fd);
  close(server_fd);
}

Test(msg, packet_roundtrips_a_sequence_of_elements)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  t_packet* packet = create_packet(OP_PACKET);
  int number = 42;
  packet_append(packet, &number, sizeof(number));
  packet_append_string(packet, "hello");
  cr_assert(send_packet(packet, client_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(server_fd), OP_PACKET);
  t_list* values = receive_packet(server_fd);
  cr_assert_eq(list_size(values), 2);
  cr_assert_eq(*(int*)list_get(values, 0), 42);
  cr_assert_str_eq((char*)list_get(values, 1), "hello");

  list_destroy_and_destroy_elements(values, free);
  close(client_fd);
  close(server_fd);
}

Test(msg, an_empty_packet_yields_an_empty_list)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  t_packet* packet = create_packet(OP_KERNEL_MEMORY_RUNNING);
  cr_assert(send_packet(packet, client_fd));
  destroy_packet(packet);

  cr_assert_eq(receive_op_code(server_fd), OP_KERNEL_MEMORY_RUNNING);
  t_list* values = receive_packet(server_fd);
  cr_assert_eq(list_size(values), 0);

  list_destroy(values);
  close(client_fd);
  close(server_fd);
}

Test(msg, receive_buffer_reports_a_zero_length_payload_as_null)
{
  int server_fd;
  int client_fd = connected_pair(&server_fd);

  int empty = 0;
  cr_assert(send_buffer(OP_MEMORY_FREED, &empty, 0, client_fd));

  cr_assert_eq(receive_op_code(server_fd), OP_MEMORY_FREED);
  int size = -1;
  void* received = receive_buffer(&size, server_fd);
  cr_assert_eq(size, 0);
  cr_assert_null(received);

  close(client_fd);
  close(server_fd);
}

#include "cpu/cleanup.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/list.h"

Test(cpu_cleanup, destroy_instruction_frees_the_name_and_parameters)
{
  t_instruction* instruction = decode_stage("SET AX 5");
  /* Leak-checked under valgrind; must not crash. */
  destroy_instruction(instruction);
}

Test(cpu_cleanup, destroy_memory_stick_closes_an_open_socket)
{
  t_socket* peer;
  t_socket* socket = cpu_connected_pair(&peer);

  t_memory_stick_info* stick = cpu_make_stick(0, 64);
  stick->socket_ms = socket;
  destroy_memory_stick(stick);

  cr_assert_eq(receive_op_code(peer), OP_CODE_ERROR);
  socket_destroy(peer);
}

Test(cpu_cleanup, iterator_close_socket_closes_and_frees_the_socket)
{
  t_socket* peer;
  t_socket* socket = cpu_connected_pair(&peer);

  iterator_close_socket(socket);

  cr_assert_eq(receive_op_code(peer), OP_CODE_ERROR);
  socket_destroy(peer);
}

Test(cpu_cleanup, close_module_tolerates_a_partially_initialised_cpu)
{
  t_cpu* cpu = calloc(1, sizeof(t_cpu));
  cpu->logger = cpu_quiet_logger();
  cpu->handlers = dictionary_create();
  /* no sockets, no config, no sticks */
  close_module(cpu); /* must not crash */
}

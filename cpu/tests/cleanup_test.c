#include "cpu/cleanup.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <unistd.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/dictionary.h"
#include "utils/collections/list.h"
#include "utils/config.h"
#include "utils/msg.h"
#include "utils/sockets.h"

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

Test(cpu_cleanup, close_module_releases_a_fully_initialised_cpu)
{
  t_cpu* cpu = calloc(1, sizeof(t_cpu));
  cpu->logger = cpu_quiet_logger();
  cpu->handlers = dictionary_create();

  t_socket* km_peer;
  cpu->socket_kernel_memory = cpu_connected_pair(&km_peer);
  t_socket* sched_peer;
  cpu->socket_kernel_scheduler = cpu_connected_pair(&sched_peer);

  t_socket* stick_peer;
  t_memory_stick_info* stick = cpu_make_stick(0, 64);
  stick->socket_ms = cpu_connected_pair(&stick_peer);
  cpu->memory_sticks = list_create();
  list_add(cpu->memory_sticks, stick);

  char* config_path = cpu_write_temp_config("LOG_LEVEL=INFO\n");
  cpu->config = config_create(config_path);
  cr_assert_not_null(cpu->config);

  close_module(cpu);

  /* every connection the cpu held is closed */
  cr_assert_eq(receive_op_code(km_peer), OP_CODE_ERROR);
  cr_assert_eq(receive_op_code(sched_peer), OP_CODE_ERROR);
  cr_assert_eq(receive_op_code(stick_peer), OP_CODE_ERROR);

  socket_destroy(km_peer);
  socket_destroy(sched_peer);
  socket_destroy(stick_peer);
  unlink(config_path);
  free(config_path);
}

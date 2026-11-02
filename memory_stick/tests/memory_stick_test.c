#include "memory_stick/memory_stick.h"

#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
#include <unistd.h>

#include "memory_stick/cpu.h"
#include "support.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"

/* ── get_args ──────────────────────────────────────────────────────────── */

Test(ms_get_args, rejects_a_wrong_argument_count)
{
  char* config_path;
  char* size_str;
  int size;
  char* two[] = {"memory_stick", "cfg"};
  char* four[] = {"memory_stick", "cfg", "1024", "extra"};
  cr_assert_not(get_args(2, two, &config_path, &size_str, &size));
  cr_assert_not(get_args(4, four, &config_path, &size_str, &size));
}

Test(ms_get_args, extracts_the_config_path_and_the_size)
{
  char* config_path = nullptr;
  char* size_str = nullptr;
  int size = 0;
  char* argv[] = {"memory_stick", "stick.config", "2048"};
  cr_assert(get_args(3, argv, &config_path, &size_str, &size));
  cr_assert_str_eq(config_path, "stick.config");
  cr_assert_str_eq(size_str, "2048");
  cr_assert_eq(size, 2048);
}

Test(ms_get_args, rejects_a_non_positive_size)
{
  char* config_path = nullptr;
  char* size_str = nullptr;
  int size = 0;
  char* zero[] = {"memory_stick", "stick.config", "0"};
  char* negative[] = {"memory_stick", "stick.config", "-1"};
  char* garbage[] = {"memory_stick", "stick.config", "abc"};
  cr_assert_not(get_args(3, zero, &config_path, &size_str, &size));
  cr_assert_not(get_args(3, negative, &config_path, &size_str, &size));
  cr_assert_not(get_args(3, garbage, &config_path, &size_str, &size));
}

/* ── read_config / init_config ─────────────────────────────────────────── */

Test(ms_config, read_config_pulls_every_key)
{
  char* path = ms_write_temp_config();
  t_config* config = config_create(path);
  cr_assert_not_null(config);

  t_config_vars vars = {0};
  read_config(config, &vars);
  cr_assert_str_eq(vars.log_level, "INFO");
  cr_assert_eq(vars.memory_delay, 0);
  cr_assert_str_eq(vars.km_ip, "127.0.0.1");
  cr_assert_str_eq(vars.km_port, "4321");

  config_destroy(config);
  unlink(path);
  free(path);
}

Test(ms_config, init_config_returns_null_for_a_missing_file)
{
  t_config_vars vars = {0};
  cr_assert_null(init_config("/no/such/stick.config", &vars));
}

/* ── write_memory / read_memory ────────────────────────────────────────── */

Test(ms_memory, write_memory_stores_the_bytes_and_confirms)
{
  t_socket* km_fd;
  t_socket* stick_fd = ms_connected_pair(&km_fd);

  t_ms* ms = ms_make(64);
  write_memory(ms, 8, "abcd", 4, stick_fd);

  cr_assert_eq(memcmp(ms->memory + 8, "abcd", 4), 0);

  cr_assert_eq(receive_op_code(km_fd), OP_MEMORY_STICK_WRITE_DONE);
  char* confirmation = receive_string(km_fd);
  cr_assert_str_eq(confirmation, "Write successful");
  free(confirmation);

  ms_destroy(ms);
  socket_destroy(stick_fd);
  socket_destroy(km_fd);
}

Test(ms_memory, read_memory_returns_the_stored_bytes)
{
  t_socket* km_fd;
  t_socket* stick_fd = ms_connected_pair(&km_fd);

  t_ms* ms = ms_make(64);
  memcpy(ms->memory + 16, "wxyz", 4);

  read_memory(ms, 16, 4, stick_fd);

  cr_assert_eq(receive_op_code(km_fd), OP_MEMORY_STICK_READ_DONE);
  int size = 0;
  char* bytes = receive_buffer(&size, km_fd);
  cr_assert_eq(size, 4);
  cr_assert_eq(memcmp(bytes, "wxyz", 4), 0);
  free(bytes);

  ms_destroy(ms);
  socket_destroy(stick_fd);
  socket_destroy(km_fd);
}

/* ── close_module_on_error ─────────────────────────────────────────────── */

Test(ms_cleanup, close_module_on_error_releases_only_what_was_acquired)
{
  t_ms ms = {0};
  ms.logger = ms_quiet_logger();
  /* no config, no memory, no mutex */
  close_module_on_error(&ms); /* must not crash */
}

Test(ms_cleanup, close_module_joins_the_cpu_thread_and_releases_everything)
{
  t_ms* ms = ms_make(16);
  ms->socket_server_cpu = create_server_cpu(ms->logger);
  cr_assert_not_null(ms->socket_server_cpu);

  thrd_t thread;
  cr_assert(start_cpu_server(&thread, ms->socket_server_cpu, ms->logger, ms));

  t_socket* km_fd;
  ms->socket_km = ms_connected_pair(&km_fd);

  char* config_path = ms_write_temp_config();
  ms->config = config_create(config_path);
  cr_assert_not_null(ms->config);

  close_module(ms, &thread); /* must not crash; joins the thread itself */

  socket_destroy(km_fd);
  unlink(config_path);
  free(config_path);
  free(ms);
}

Test(ms_cleanup, close_module_on_error_releases_everything_that_was_acquired)
{
  t_ms* ms = ms_make(16);
  ms->socket_server_cpu = create_server_cpu(ms->logger);
  cr_assert_not_null(ms->socket_server_cpu);
  t_socket* km_fd;
  ms->socket_km = ms_connected_pair(&km_fd);

  char* config_path = ms_write_temp_config();
  ms->config = config_create(config_path);
  cr_assert_not_null(ms->config);

  /* logger, config, memory and its mutex are all set: each gets released. */
  close_module_on_error(ms);

  socket_destroy(km_fd);
  unlink(config_path);
  free(config_path);
  free(ms);
}

/* ── init_logger ────────────────────────────────────────────────────────── */

Test(ms_init, init_logger_builds_a_working_logger)
{
  t_log* logger = init_logger(LOG_LEVEL_INFO);
  cr_assert_not_null(logger);
  log_destroy(logger);
  unlink("memory_stick.log"); /* init_logger hardcodes this filename */
}

/* ── send_cpu_server_port ──────────────────────────────────────────────── */

Test(ms_init, send_cpu_server_port_reports_the_listening_port)
{
  t_log* logger = ms_quiet_logger();
  t_socket* km_fd;
  t_socket* stick_fd = ms_connected_pair(&km_fd);

  t_socket* listen_socket = create_server_cpu(logger);
  cr_assert_not_null(listen_socket);

  cr_assert(send_cpu_server_port(stick_fd, listen_socket, logger));

  cr_assert_eq(receive_op_code(km_fd), OP_PORT);
  char* port = receive_string(km_fd);
  cr_assert_gt(atoi(port), 0);
  free(port);

  socket_destroy(listen_socket);
  socket_destroy(stick_fd);
  socket_destroy(km_fd);
  log_destroy(logger);
}

/* ── init_module ───────────────────────────────────────────────────────── */

/* init_module logs to "memory_stick.log" in the working directory; run it
 * from a scratch directory so the test leaves nothing behind. */
static char init_scratch[] = "/tmp/ms_init_cwd_XXXXXX";

static void enter_scratch_dir(void)
{
  cr_redirect_stdout();
  strcpy(init_scratch, "/tmp/ms_init_cwd_XXXXXX");
  cr_assert_not_null(mkdtemp(init_scratch));
  cr_assert_eq(chdir(init_scratch), 0);
}

static void leave_scratch_dir(void)
{
  unlink("memory_stick.log");
  cr_assert_eq(chdir("/"), 0);
  rmdir(init_scratch);
}

static char* write_config_for_port(const char* km_port)
{
  char path[] = "/tmp/ms_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1);
  FILE* file = fdopen(fd, "w");
  fprintf(file,
          "LOG_LEVEL=ERROR\n"
          "MEMORY_DELAY=3\n"
          "KERNEL_MEMORY_IP=127.0.0.1\n"
          "KERNEL_MEMORY_PORT=%s\n",
          km_port);
  fclose(file);
  return strdup(path);
}

struct km_stub
{
  t_socket* listener;
  char* size;
  char* cpu_port;
};

/* Plays Kernel Memory's side of a stick's start-up: handshake, size, then
 * the CPU server port. */
static int km_stub_thread(void* arg)
{
  struct km_stub* stub = arg;
  t_socket* stick = socket_accept(stub->listener, false);
  if (stick == nullptr)
  {
    return 0;
  }
  receive_handshake(stick);
  send_handshake(MID_KERNEL_MEMORY, stick);
  if (receive_op_code(stick) == OP_MEMORY_SIZE)
  {
    stub->size = receive_string(stick);
  }
  if (receive_op_code(stick) == OP_PORT)
  {
    stub->cpu_port = receive_string(stick);
  }
  socket_destroy(stick);
  return 0;
}

Test(ms_init_module, fails_when_the_config_file_is_missing)
{
  t_ms ms = {0};
  thrd_t thread;
  cr_assert_not(
      init_module(&ms, "/no/such/directory/ms.config", "64", &thread));
  cr_assert_null(ms.config);
}

Test(ms_init_module, fails_when_kernel_memory_is_unreachable,
     .init = enter_scratch_dir, .fini = leave_scratch_dir)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, nullptr, SOCKET_PORT_EPHEMERAL, false);
  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));
  socket_destroy(listener); /* nothing listens there any more */
  char* config_path = write_config_for_port(port);

  t_ms ms = {0};
  thrd_t thread;
  cr_assert_not(init_module(&ms, config_path, "64", &thread));
  cr_assert_not_null(ms.config);
  cr_assert_not_null(ms.logger);
  cr_assert_null(ms.socket_km);

  close_module_on_error(&ms);
  unlink(config_path);
  free(config_path);
}

Test(ms_init_module, connects_reports_its_geometry_and_starts_the_cpu_server,
     .init = enter_scratch_dir, .fini = leave_scratch_dir, .timeout = 10.0)
{
  t_socket* listener =
      socket_create(SOCKET_KIND_SERVER, nullptr, SOCKET_PORT_EPHEMERAL, false);
  cr_assert_not_null(listener);
  char port[16];
  snprintf(port, sizeof(port), "%d", socket_get_local_port(listener));
  char* config_path = write_config_for_port(port);

  struct km_stub stub = {.listener = listener};
  thrd_t km_thread;
  thrd_create(&km_thread, km_stub_thread, &stub);

  t_ms ms = {0};
  thrd_t cpu_server_thread;
  cr_assert(init_module(&ms, config_path, "64", &cpu_server_thread));

  thrd_join(km_thread, nullptr);
  cr_assert_str_eq(stub.size, "64");
  cr_assert_eq(atoi(stub.cpu_port), get_cpu_port(ms.socket_server_cpu));
  cr_assert_eq(ms.memory_delay, 3);
  cr_assert_not_null(ms.memory);
  cr_assert_not_null(ms.memory_mutex);

  close_module(&ms, &cpu_server_thread);
  free(stub.size);
  free(stub.cpu_port);
  socket_destroy(listener);
  unlink(config_path);
  free(config_path);
}

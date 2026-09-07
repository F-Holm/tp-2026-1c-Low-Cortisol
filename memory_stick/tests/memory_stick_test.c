#include "memory_stick/memory_stick.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

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
  char* config_path = NULL;
  char* size_str = NULL;
  int size = 0;
  char* argv[] = {"memory_stick", "stick.config", "2048"};
  cr_assert(get_args(3, argv, &config_path, &size_str, &size));
  cr_assert_str_eq(config_path, "stick.config");
  cr_assert_str_eq(size_str, "2048");
  cr_assert_eq(size, 2048);
}

Test(ms_get_args, rejects_a_non_positive_size)
{
  char* config_path = NULL;
  char* size_str = NULL;
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
  int km_fd;
  int stick_fd = ms_connected_pair(&km_fd);

  t_ms* ms = ms_make(64);
  write_memory(ms, 8, "abcd", 4, stick_fd);

  cr_assert_eq(memcmp(ms->memory + 8, "abcd", 4), 0);

  cr_assert_eq(receive_op_code(km_fd), OP_MEMORY_STICK_WRITE_DONE);
  char* confirmation = receive_string(km_fd);
  cr_assert_str_eq(confirmation, "Write successful");
  free(confirmation);

  ms_destroy(ms);
  close(stick_fd);
  close(km_fd);
}

Test(ms_memory, read_memory_returns_the_stored_bytes)
{
  int km_fd;
  int stick_fd = ms_connected_pair(&km_fd);

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
  close(stick_fd);
  close(km_fd);
}

/* ── close_module_on_error ─────────────────────────────────────────────── */

Test(ms_cleanup, close_module_on_error_releases_only_what_was_acquired)
{
  t_ms ms = {0};
  ms.logger = ms_quiet_logger();
  ms.socket_km = -1;
  ms.socket_server_cpu = -1;
  /* no config, no memory, no mutex */
  close_module_on_error(&ms); /* must not crash */
}

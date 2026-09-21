#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"
#include "swap/swap.h"
#include "utils/config.h"
#include "utils/log.h"
#include "utils/msg.h"
#include "utils/sockets.h"
#include "utils/swap_km.h"
#include "utils/threads.h"

/* ── init_config ───────────────────────────────────────────────────────── */

static char* make_swap_file_path(void)
{
  char path[] = "/tmp/swap_test_file_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1);
  close(fd);
  return strdup(path);
}

Test(swap_init_config, loads_the_config_and_prepares_the_swap_file)
{
  char* swap_file = make_swap_file_path();
  char* config_path = swap_write_temp_config(swap_file);

  char scratch[] = "/tmp/swap_test_cwd_XXXXXX";
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);

  t_config* config = config_create(config_path);
  cr_assert_not_null(config);

  t_swap swap = {0};
  cr_assert(init_config(&swap, config));

  cr_assert_str_eq(swap.ip, "127.0.0.1");
  cr_assert_str_eq(swap.port, "1234");
  cr_assert_eq(swap.swap_size, 4096);
  cr_assert_eq(swap.block_size, 64);
  cr_assert_not_null(swap.logger);
  cr_assert_not_null(swap.swap_file);

  struct stat info;
  cr_assert_eq(stat(swap_file, &info), 0);
  cr_assert_eq(info.st_size, 4096);

  fclose(swap.swap_file);
  log_destroy(swap.logger);
  config_destroy(config);
  unlink("swap.log");
  cr_assert_eq(chdir("/"), 0);
  rmdir(scratch);
  unlink(swap_file);
  unlink(config_path);
  free(swap_file);
  free(config_path);
}

Test(swap_init_config, fails_and_frees_the_config_when_the_log_file_cannot_open)
{
  if (geteuid() == 0)
  {
    cr_skip("needs a non-root user to hit a permission error");
  }
  char* swap_file = make_swap_file_path();
  char* config_path = swap_write_temp_config(swap_file);

  char scratch[] = "/tmp/swap_test_ro_XXXXXX";
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);
  cr_assert_eq(chmod(scratch, 0500), 0);

  t_config* config = config_create(config_path);
  cr_assert_not_null(config);

  t_swap swap = {0};
  cr_assert_not(init_config(&swap, config)); /* init_config destroys config */

  cr_assert_eq(chdir("/"), 0);
  chmod(scratch, 0700);
  rmdir(scratch);
  unlink(swap_file);
  unlink(config_path);
  free(swap_file);
  free(config_path);
}

Test(swap_init_config, fails_cleanly_when_the_swap_file_cannot_be_created,
     .init = cr_redirect_stdout)
{
  char* config_path = swap_write_temp_config("/no/such/directory/swap.bin");

  char scratch[] = "/tmp/swap_test_cwd_XXXXXX";
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);

  t_config* config = config_create(config_path);
  cr_assert_not_null(config);

  t_swap swap = {0};
  /* close_swap runs here with no socket and no swap file open; it must not
   * crash and init_config must report the failure. */
  cr_assert_not(init_config(&swap, config));

  unlink("swap.log");
  cr_assert_eq(chdir("/"), 0);
  rmdir(scratch);
  unlink(config_path);
  free(config_path);
}

/* ── connect_to_kernel_memory ──────────────────────────────────────────── */

struct km_stub
{
  t_socket* listener;
  int swap_module_id; /* module id the stub received from swap */
  int announce_as;    /* module id the stub sends back */
  bool got_info;      /* whether the stub received OP_INFO_SWAP */
  t_swap_config info; /* the sizes swap reported */
};

static void* km_stub_thread(void* arg)
{
  struct km_stub* stub = arg;
  t_socket* client = socket_accept(stub->listener, false);
  if (client == NULL)
  {
    return NULL;
  }
  stub->swap_module_id = receive_handshake(client);
  send_handshake(stub->announce_as, client);
  if (receive_op_code(client) == OP_INFO_SWAP)
  {
    int size;
    t_swap_config* received = receive_buffer(&size, client);
    if (received != NULL)
    {
      stub->info = *received;
      stub->got_info = true;
      free(received);
    }
  }
  socket_destroy(client);
  return NULL;
}

Test(swap_connect, completes_the_handshake_and_reports_its_sizes)
{
  char port[16];
  t_socket* listener = swap_listen_ephemeral(port, sizeof(port));

  struct km_stub stub = {
      .listener = listener,
      .swap_module_id = -1,
      .announce_as = MID_KERNEL_MEMORY,
  };
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);

  t_swap swap = {0};
  swap.logger = swap_quiet_logger();
  swap.ip = "127.0.0.1";
  swap.port = port;
  swap.swap_size = 8192;
  swap.block_size = 128;

  cr_assert(
      connect_to_kernel_memory(&swap, NULL)); /* config unused on success */

  thrd_join(thread, NULL);
  cr_assert_eq(stub.swap_module_id, MID_SWAP);
  cr_assert(stub.got_info);
  cr_assert_eq(stub.info.swap_size, 8192);
  cr_assert_eq(stub.info.block_size, 128);

  socket_destroy(swap.socket_swap);
  socket_destroy(listener);
  log_destroy(swap.logger);
}

Test(swap_connect, fails_when_the_peer_is_not_kernel_memory)
{
  char port[16];
  t_socket* listener = swap_listen_ephemeral(port, sizeof(port));

  struct km_stub stub = {
      .listener = listener,
      .swap_module_id = -1,
      .announce_as = MID_CPU, /* wrong */
  };
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);

  t_swap swap = {0};
  swap.swap_file = swap_sized_tmpfile(64); /* close_swap will fclose it */
  swap.logger = swap_quiet_logger();
  swap.ip = "127.0.0.1";
  swap.port = port;

  t_config* config = config_create((char*)"/dev/null");
  cr_assert_not(connect_to_kernel_memory(&swap, config)); /* close_swap runs */

  thrd_join(thread, NULL);
  cr_assert_eq(stub.swap_module_id, MID_SWAP);

  socket_destroy(listener);
}

Test(swap_connect, fails_when_kernel_memory_is_unreachable)
{
  char port[16];
  t_socket* listener = swap_listen_ephemeral(port, sizeof(port));
  socket_destroy(listener);

  t_swap swap = {0};
  swap.swap_file = swap_sized_tmpfile(64);
  swap.logger = swap_quiet_logger();
  swap.ip = "127.0.0.1";
  swap.port = port;

  t_config* config = config_create((char*)"/dev/null");
  cr_assert_not(connect_to_kernel_memory(&swap, config));
}

/* ── close_swap ────────────────────────────────────────────────────────── */

Test(swap_lifecycle, close_swap_closes_the_socket_and_releases_the_resources)
{
  char* config_path = swap_write_temp_config("/tmp/unused");
  t_config* config = config_create(config_path);
  cr_assert_not_null(config);

  t_socket* peer;
  t_socket* socket = swap_connected_pair(&peer);

  t_swap swap = {0};
  swap.socket_swap = socket;
  swap.swap_file = tmpfile();
  swap.logger = swap_quiet_logger();

  close_swap(&swap, config);

  cr_assert_eq(receive_op_code(peer), OP_CODE_ERROR,
               "the peer should see the socket closed");

  socket_destroy(peer);
  unlink(config_path);
  free(config_path);
}

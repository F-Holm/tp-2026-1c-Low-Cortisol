#include <criterion/criterion.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"
#include "swap/swap.h"
#include "utils/msg.h"
#include "utils/swap_km.h"

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
  swap.socket_swap = -1;
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
  swap.socket_swap = -1;
  cr_assert_not(init_config(&swap, config)); /* init_config destroys config */

  cr_assert_eq(chdir("/"), 0);
  chmod(scratch, 0700);
  rmdir(scratch);
  unlink(swap_file);
  unlink(config_path);
  free(swap_file);
  free(config_path);
}

/* ── connect_to_kernel_memory ──────────────────────────────────────────── */

struct km_stub
{
  int listen_fd;
  int swap_module_id; /* module id the stub received from swap */
  int announce_as;    /* module id the stub sends back */
  bool got_info;      /* whether the stub received OP_INFO_SWAP */
  t_swap_config info; /* the sizes swap reported */
};

static void* km_stub_thread(void* arg)
{
  struct km_stub* stub = arg;
  int client = accept(stub->listen_fd, NULL, NULL);
  if (client < 0)
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
  close(client);
  return NULL;
}

Test(swap_connect, completes_the_handshake_and_reports_its_sizes)
{
  char port[16];
  int listen_fd = swap_listen_ephemeral(port, sizeof(port));

  struct km_stub stub = {
      .listen_fd = listen_fd,
      .swap_module_id = -1,
      .announce_as = MID_KERNEL_MEMORY,
  };
  pthread_t thread;
  pthread_create(&thread, NULL, km_stub_thread, &stub);

  t_swap swap = {0};
  swap.socket_swap = -1;
  swap.logger = swap_quiet_logger();
  swap.ip = "127.0.0.1";
  swap.port = port;
  swap.swap_size = 8192;
  swap.block_size = 128;

  cr_assert(
      connect_to_kernel_memory(&swap, NULL)); /* config unused on success */

  pthread_join(thread, NULL);
  cr_assert_eq(stub.swap_module_id, MID_SWAP);
  cr_assert(stub.got_info);
  cr_assert_eq(stub.info.swap_size, 8192);
  cr_assert_eq(stub.info.block_size, 128);

  close(swap.socket_swap);
  close(listen_fd);
  log_destroy(swap.logger);
}

Test(swap_connect, fails_when_the_peer_is_not_kernel_memory)
{
  char port[16];
  int listen_fd = swap_listen_ephemeral(port, sizeof(port));

  struct km_stub stub = {
      .listen_fd = listen_fd,
      .swap_module_id = -1,
      .announce_as = MID_CPU, /* wrong */
  };
  pthread_t thread;
  pthread_create(&thread, NULL, km_stub_thread, &stub);

  t_swap swap = {0};
  swap.socket_swap = -1;
  swap.swap_file = swap_sized_tmpfile(64); /* close_swap will fclose it */
  swap.logger = swap_quiet_logger();
  swap.ip = "127.0.0.1";
  swap.port = port;

  t_config* config = config_create((char*)"/dev/null");
  cr_assert_not(connect_to_kernel_memory(&swap, config)); /* close_swap runs */

  pthread_join(thread, NULL);
  cr_assert_eq(stub.swap_module_id, MID_SWAP);

  close(listen_fd);
}

Test(swap_connect, fails_when_kernel_memory_is_unreachable)
{
  char port[16];
  int listen_fd = swap_listen_ephemeral(port, sizeof(port));
  close(listen_fd);

  t_swap swap = {0};
  swap.socket_swap = -1;
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

  int fds[2];
  cr_assert_eq(pipe(fds), 0);

  t_swap swap = {0};
  swap.socket_swap = fds[0];
  swap.swap_file = tmpfile();
  swap.logger = swap_quiet_logger();

  close_swap(&swap, config);

  cr_assert_eq(fcntl(fds[0], F_GETFD), -1, "socket fd should be closed");
  cr_assert_eq(errno, EBADF);

  close(fds[1]);
  unlink(config_path);
  free(config_path);
}

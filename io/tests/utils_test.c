#include "io/utils.h"

#include <criterion/criterion.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "support.h"
#include "utils/msg.h"

/* ── parse_args ─────────────────────────────────────────────────────────── */

Test(io_parse_args, rejects_a_wrong_argument_count)
{
  t_io io = {0};
  char* too_few[] = {"io", "config.ini"};
  char* too_many[] = {"io", "config.ini", "STDIN", "extra"};
  cr_assert_not(parse_args(2, too_few, &io));
  cr_assert_not(parse_args(4, too_many, &io));
}

Test(io_parse_args, recognises_every_interface_type)
{
  char* config = io_write_temp_config();

  struct
  {
    char* name;
    int type;
  } cases[] = {
      {"STDIN", E_STDIN},
      {"STDOUT", E_STDOUT},
      {"SLEEP", E_SLEEP},
  };

  for (int i = 0; i < 3; i++)
  {
    t_io io = {0};
    char* argv[] = {"io", config, cases[i].name};
    cr_assert(parse_args(3, argv, &io), "type %s should parse", cases[i].name);
    cr_assert_eq(io.io_type, cases[i].type);
    config_destroy(io.config);
  }

  unlink(config);
  free(config);
}

Test(io_parse_args, rejects_an_unknown_interface_type)
{
  char* config = io_write_temp_config();
  t_io io = {0};
  char* argv[] = {"io", config, "TELEPATHY"};
  cr_assert_not(parse_args(3, argv, &io));
  config_destroy(io.config);
  unlink(config);
  free(config);
}

/* ── load_config ───────────────────────────────────────────────────────── */

Test(io_load_config, reads_the_scheduler_endpoint_and_builds_a_logger)
{
  char* config = io_write_temp_config();

  char scratch[] = "/tmp/io_test_cwd_XXXXXX";
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);

  t_io io = {0};
  io.config = config_create(config);
  cr_assert_not_null(io.config);

  cr_assert(load_config(&io));
  cr_assert_str_eq(io.ip, "127.0.0.1");
  cr_assert_str_eq(io.port, "1234");
  cr_assert_not_null(io.logger);
  cr_assert_not_null(io.logger->file); /* "io.log" opened in the scratch dir */

  log_destroy(io.logger);
  config_destroy(io.config);
  unlink("io.log");
  cr_assert_eq(chdir("/"), 0);
  rmdir(scratch);
  unlink(config);
  free(config);
}

Test(io_load_config, fails_and_frees_the_config_when_the_log_file_cannot_open)
{
  if (geteuid() == 0)
  {
    cr_skip("needs a non-root user to hit a permission error");
  }
  char* config = io_write_temp_config();

  char scratch[] = "/tmp/io_test_ro_XXXXXX";
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);
  cr_assert_eq(chmod(scratch, 0500), 0); /* read + execute, no write */

  t_io io = {0};
  io.config = config_create(config);
  cr_assert_not_null(io.config);

  cr_assert_not(load_config(&io)); /* load_config destroys io.config itself */

  cr_assert_eq(chdir("/"), 0);
  chmod(scratch, 0700);
  rmdir(scratch);
  unlink(config);
  free(config);
}

/* ── connect_to_scheduler ──────────────────────────────────────────────── */

struct scheduler_stub
{
  int listen_fd;
  int io_module_id;  /* module id the stub received from io */
  char* io_type;     /* IO type string io announced, strdup'd */
  int announce_as;   /* module id the stub sends back */
  bool reached_type; /* whether the stub got as far as the type message */
};

static void* scheduler_stub_thread(void* arg)
{
  struct scheduler_stub* stub = arg;
  int client = accept(stub->listen_fd, NULL, NULL);
  if (client < 0)
  {
    return NULL;
  }
  stub->io_module_id = receive_handshake(client);
  send_handshake(stub->announce_as, client);
  if (receive_op_code(client) == OP_IO_TYPE)
  {
    stub->io_type = receive_string(client);
    stub->reached_type = true;
  }
  close(client);
  return NULL;
}

Test(io_connect, completes_the_handshake_and_announces_its_type)
{
  char port[16];
  int listen_fd = io_listen_ephemeral(port, sizeof(port));

  struct scheduler_stub stub = {
      .listen_fd = listen_fd,
      .io_module_id = -1,
      .announce_as = MID_KERNEL_SCHEDULER,
  };
  pthread_t thread;
  pthread_create(&thread, NULL, scheduler_stub_thread, &stub);

  t_io io = {0};
  io.logger = io_quiet_logger();
  io.ip = "127.0.0.1";
  io.port = port;
  io.io_type = E_STDOUT;

  cr_assert(connect_to_scheduler(&io));

  pthread_join(thread, NULL);
  cr_assert_eq(stub.io_module_id, MID_IO);
  cr_assert(stub.reached_type);
  cr_assert_str_eq(stub.io_type, "STDOUT");

  free(stub.io_type);
  close(io.socket_io);
  close(listen_fd);
  log_destroy(io.logger);
}

Test(io_connect, fails_when_the_peer_is_not_the_kernel_scheduler)
{
  char port[16];
  int listen_fd = io_listen_ephemeral(port, sizeof(port));

  struct scheduler_stub stub = {
      .listen_fd = listen_fd,
      .io_module_id = -1,
      .announce_as = MID_CPU, /* wrong: io wants MID_KERNEL_SCHEDULER */
  };
  pthread_t thread;
  pthread_create(&thread, NULL, scheduler_stub_thread, &stub);

  t_io io = {0};
  io.config = config_create((char*)"/dev/null"); /* non-NULL for close_io */
  io.logger = io_quiet_logger();
  io.ip = "127.0.0.1";
  io.port = port;
  io.io_type = E_SLEEP;

  cr_assert_not(connect_to_scheduler(&io)); /* close_io runs on this path */

  pthread_join(thread, NULL);
  cr_assert_eq(stub.io_module_id, MID_IO);

  close(listen_fd);
}

Test(io_connect, fails_when_the_scheduler_is_unreachable)
{
  char port[16];
  int listen_fd = io_listen_ephemeral(port, sizeof(port));
  close(listen_fd); /* nothing is listening on this port any more */

  t_io io = {0};
  io.config = config_create((char*)"/dev/null");
  io.logger = io_quiet_logger();
  io.ip = "127.0.0.1";
  io.port = port;
  io.io_type = E_STDIN;

  cr_assert_not(connect_to_scheduler(&io));
}

/* ── close_io ──────────────────────────────────────────────────────────── */

Test(io_utils, close_io_closes_the_socket_and_releases_the_resources)
{
  char* config = io_write_temp_config();

  int fds[2];
  cr_assert_eq(pipe(fds), 0);

  t_io io = {0};
  io.config = config_create(config);
  io.logger = io_quiet_logger();
  io.socket_io = fds[0];

  close_io(&io);

  cr_assert_eq(fcntl(fds[0], F_GETFD), -1, "socket fd should be closed");
  cr_assert_eq(errno, EBADF);

  close(fds[1]);
  unlink(config);
  free(config);
}

#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <threads.h>
#include <unistd.h>

#include "kernel_scheduler/app/kernel_scheduler.h"
#include "support.h"
#include "utils/msg.h"
#include "utils/sockets.h"

/* start_module logs to "kernel_scheduler.log" in the working directory, and
 * to stdout: run every test from a scratch directory with stdout captured. */
static char scratch[] = "/tmp/ks_app_cwd_XXXXXX";

static void enter_scratch_dir(void)
{
  cr_redirect_stdout();
  strcpy(scratch, "/tmp/ks_app_cwd_XXXXXX");
  cr_assert_not_null(mkdtemp(scratch));
  cr_assert_eq(chdir(scratch), 0);
}

static void leave_scratch_dir(void)
{
  unlink("kernel_scheduler.log");
  cr_assert_eq(chdir("/"), 0);
  chmod(scratch, 0700);
  rmdir(scratch);
}

TestSuite(ks_app, .init = enter_scratch_dir, .fini = leave_scratch_dir,
          .timeout = 10.0);

static char* write_config(const char* km_port, const char* server_port)
{
  char path[] = "/tmp/ks_app_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1);
  FILE* file = fdopen(fd, "w");
  fprintf(file,
          "LOG_LEVEL=ERROR\n"
          "SCHEDULING_ALGORITHM=MULTILEVEL\n"
          "QUEUE_ALGORITHMS=[FIFO,RR]\n"
          "RR_QUANTUM=100\n"
          "QUEUE_PREEMPTION=TRUE\n"
          "SUSPENSION_TIMEOUT=1000\n"
          "KERNEL_MEMORY_IP=127.0.0.1\n"
          "KERNEL_MEMORY_PORT=%s\n"
          "KERNEL_SCHEDULER_PORT=%s\n",
          km_port, server_port);
  fclose(file);
  return strdup(path);
}

/* A Kernel Memory that accepts one scheduler, answers its handshake, then
 * swallows whatever it sends (watchdog pings) until it hangs up. */
typedef struct
{
  t_socket* listener;
  t_socket* scheduler;
} t_km_stub;

static int km_stub_thread(void* arg)
{
  t_km_stub* stub = arg;
  stub->scheduler = socket_accept(stub->listener, false);
  if (stub->scheduler == nullptr)
  {
    return 0;
  }
  receive_handshake(stub->scheduler);
  send_handshake(MID_KERNEL_MEMORY, stub->scheduler);
  while (receive_op_code(stub->scheduler) != OP_CODE_ERROR)
  {
    free(receive_string(stub->scheduler));
  }
  return 0;
}

static void closed_port(char* port, int len)
{
  socket_destroy(ks_listen_ephemeral(port, len));
}

Test(ks_app, start_module_fails_on_a_missing_config)
{
  t_kernel_scheduler resources = {0};
  cr_assert_not(start_module(&resources, "/no/such/kernel_scheduler.config"));
}

Test(ks_app, start_module_fails_when_the_log_file_cannot_be_created)
{
  char* config_path = write_config("1", "0");
  cr_assert_eq(chmod(scratch, 0500), 0); /* nobody may create the log */

  t_kernel_scheduler resources = {0};
  cr_assert_not(start_module(&resources, config_path));

  unlink(config_path);
  free(config_path);
}

Test(ks_app, start_module_fails_when_kernel_memory_is_unreachable)
{
  char km_port[16];
  closed_port(km_port, sizeof(km_port));
  char* config_path = write_config(km_port, "0");

  t_kernel_scheduler resources = {0};
  cr_assert_not(start_module(&resources, config_path));

  unlink(config_path);
  free(config_path);
}

Test(ks_app, start_module_fails_when_its_server_cannot_listen)
{
  char km_port[16];
  t_km_stub stub = {.listener = ks_listen_ephemeral(km_port, 16)};
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);
  char* config_path = write_config(km_port, "not-a-port");

  t_kernel_scheduler resources = {0};
  cr_assert_not(start_module(&resources, config_path));

  /* start_module closed its Kernel Memory connection on the way out */
  thrd_join(thread, nullptr);
  socket_destroy(stub.scheduler);
  socket_destroy(stub.listener);
  unlink(config_path);
  free(config_path);
}

Test(ks_app, starts_initializes_and_closes_the_whole_module)
{
  char km_port[16];
  t_km_stub stub = {.listener = ks_listen_ephemeral(km_port, 16)};
  thrd_t thread;
  thrd_create(&thread, km_stub_thread, &stub);
  char* config_path = write_config(km_port, "0");

  t_kernel_scheduler resources = {0};
  cr_assert(start_module(&resources, config_path));
  cr_assert_eq(resources.config_vars.scheduling_algorithm,
               SA_MULTILEVEL_QUEUES);
  cr_assert_eq(list_size(resources.config_vars.multilevel_algorithms), 2);
  cr_assert_eq(*(int*)list_get(resources.config_vars.multilevel_algorithms, 1),
               SA_RR);
  cr_assert_eq(resources.config_vars.rr_quantum, 100);
  cr_assert(resources.config_vars.preemption);
  cr_assert_eq(resources.config_vars.suspension_timeout, 1000);
  cr_assert_not_null(resources.socket_server);

  init_scheduler_resources(&resources);
  cr_assert_not_null(resources.queues);
  cr_assert_not_null(resources.mutex_list);
  cr_assert_not_null(resources.connection_check_thread_data);

  close_module(&resources);

  thrd_join(thread, nullptr);
  socket_destroy(stub.scheduler);
  socket_destroy(stub.listener);
  unlink(config_path);
  free(config_path);
}

#include <criterion/criterion.h>
#include <pthread.h>
#include <unistd.h>

#include "kernel_scheduler/connections/cpu.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/msg.h"

Test(ks_cpu_connection, succeeds_and_registers_a_worker_thread)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "cpu1", client_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues_full(logger);
  t_mutex_list* mutex_list = init_list_mutex();
  t_io* io = create_io_structures();
  t_list* list_sockets_cpu = list_create();
  pthread_mutex_t mutex_list_sockets_cpu;
  pthread_cond_t cpu_done_cond;
  pthread_mutex_init(&mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&cpu_done_cond, NULL);

  cr_assert(handle_new_cpu(server_fd, list_sockets_cpu, &mutex_list_sockets_cpu,
                           &cpu_done_cond, logger, mutex_list, queues, io,
                           queues->km_socket, -1));
  cr_assert_eq(receive_handshake(client_fd), MID_KERNEL_SCHEDULER);
  cr_assert_eq(list_size(list_sockets_cpu), 1);

  /* The worker thread blocks waiting for a ready process; terminating the
   * ready queue wakes it up so it shuts itself down and removes itself from
   * the list, exactly as happens during a real Kernel Scheduler shutdown. */
  close_cpu(list_sockets_cpu, &mutex_list_sockets_cpu, &cpu_done_cond, queues);

  close(client_fd);
  destroy_list_mutex(mutex_list);
  close_io(io);
  ks_destroy_stub_queues_full(queues);
  log_destroy(logger);
}

Test(ks_cpu_connection, fails_when_the_id_handshake_is_wrong)
{
  int server_fd;
  int client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_KERNEL_MEMORY_RUNNING, "not-an-id", client_fd));

  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_mutex_list* mutex_list = init_list_mutex();
  t_io* io = create_io_structures();
  t_list* list_sockets_cpu = list_create();
  pthread_mutex_t mutex_list_sockets_cpu;
  pthread_cond_t cpu_done_cond;
  pthread_mutex_init(&mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&cpu_done_cond, NULL);

  cr_assert_not(handle_new_cpu(server_fd, list_sockets_cpu,
                               &mutex_list_sockets_cpu, &cpu_done_cond, logger,
                               mutex_list, queues, io, NULL, -1));
  cr_assert(list_is_empty(list_sockets_cpu));

  close(server_fd);
  close(client_fd);
  list_destroy(list_sockets_cpu);
  pthread_mutex_destroy(&mutex_list_sockets_cpu);
  pthread_cond_destroy(&cpu_done_cond);
  destroy_list_mutex(mutex_list);
  close_io(io);
  free(queues);
  log_destroy(logger);
}

Test(ks_cpu_connection, fails_gracefully_on_a_dead_socket)
{
  t_log* logger = ks_quiet_logger();
  t_queues* queues = ks_stub_queues(logger);
  t_mutex_list* mutex_list = init_list_mutex();
  t_io* io = create_io_structures();
  t_list* list_sockets_cpu = list_create();
  pthread_mutex_t mutex_list_sockets_cpu;
  pthread_cond_t cpu_done_cond;
  pthread_mutex_init(&mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&cpu_done_cond, NULL);

  cr_assert_not(handle_new_cpu(-1, list_sockets_cpu, &mutex_list_sockets_cpu,
                               &cpu_done_cond, logger, mutex_list, queues, io,
                               NULL, -1));

  list_destroy(list_sockets_cpu);
  pthread_mutex_destroy(&mutex_list_sockets_cpu);
  pthread_cond_destroy(&cpu_done_cond);
  destroy_list_mutex(mutex_list);
  close_io(io);
  free(queues);
  log_destroy(logger);
}

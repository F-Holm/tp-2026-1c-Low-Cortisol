#include <criterion/criterion.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>

#include "kernel_scheduler/connections/cpu.h"
#include "kernel_scheduler/domain/pcb.h"
#include "kernel_scheduler/scheduler/exec_list.h"
#include "kernel_scheduler/scheduler/ready_queue.h"
#include "kernel_scheduler/syscalls/mutex.h"
#include "support.h"
#include "utils/collections/list.h"
#include "utils/io.h"
#include "utils/msg.h"
#include "utils/syscalls.h"

TestSuite(ks_cpu_connection, .timeout = 10.0);

/* ── shared helpers for the syscall-dispatch tests below ─────────────────
 *
 * Every test in this section drives the real (static) handle_cpu_client
 * dispatch loop end-to-end: a "CPU" peer on `client_fd` completes the
 * OP_ID_CPU handshake, handle_new_cpu() spawns a real detached thread, a pcb
 * is already sitting in queues->ready for that thread to pick up, and the
 * test plays the CPU side of one syscall round-trip before tearing
 * everything down with close_cpu(). */

/** @brief Creates a pcb in EST_READY and places it directly in the ready
 *         queue, standing in for a process the dispatch loop's
 *         transition_take_ready_blocking() will hand to the worker thread. */
static t_pcb* seed_ready_pcb(t_queues* queues, int priority)
{
  t_pcb* pcb = create_pcb(EST_READY, priority);
  transition_to_ready(pcb, &(queues->ready));
  return pcb;
}

/** @brief Reads the OP_RESUME_PROCESS + pid the dispatch loop sends every
 *         time it hands a pcb to the CPU. */
static uint32_t drain_resume_pid(int fd)
{
  cr_assert_eq(receive_op_code(fd), OP_RESUME_PROCESS);
  int size;
  uint32_t* buffer = receive_buffer(&size, fd);
  uint32_t pid = *buffer;
  free(buffer);
  return pid;
}

/** @brief Reads the OP_INTERRUPT/OP_NO_INTERRUPT + reason send_preemption()
 *         sends after every handled syscall, and checks both. */
static void expect_preemption(int fd, int expected_op_code,
                              const char* expected_reason)
{
  cr_assert_eq(receive_op_code(fd), expected_op_code);
  char* reason = receive_string(fd);
  cr_assert_str_eq(reason, expected_reason);
  free(reason);
}

/** @brief For syscalls that do NOT preempt (PR_NO_PREEMPTION): the pcb stays
 *         in EXEC forever from the dispatch loop's point of view, so the
 *         test must reclaim and destroy it itself after close_cpu(). */
static void reap_exec_leftover(t_queues* queues, t_pcb* expected)
{
  t_pcb* leftover = transition_take_exec_next(&(queues->exec));
  cr_assert_eq(leftover, expected);
  destroy_pcb(leftover);
}

/** @brief Wires queues->km_socket to a real fake "Kernel Memory" peer that
 *         immediately answers OP_REQUEST_PROCESS_SIZE with size 0 (so the
 *         exit path skips the resumption-routine kick) -- enough for any
 *         syscall handler that ends in transition_exec_exit(). Returns the
 *         peer's fd (close it at the end of the test). */
static int wire_km_for_exit(t_queues* queues)
{
  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);
  int size = 0;
  cr_assert(send_buffer(OP_PROCESS_SIZE, &size, sizeof(size), km_server_fd));
  queues->km_socket->km_socket = km_client_fd;
  return km_server_fd;
}

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

/* ── syscall dispatch (handle_cpu_client) ─────────────────────────────────
 *
 * Common fixture: a real pcb sitting in queues->ready, a real handle_new_cpu
 * connection, and the test playing the CPU peer for exactly one syscall
 * round-trip before shutting everything down. */

typedef struct
{
  t_log* logger;
  t_queues* queues;
  t_mutex_list* mutex_list;
  t_io* io;
  t_list* list_sockets_cpu;
  pthread_mutex_t mutex_list_sockets_cpu;
  pthread_cond_t cpu_done_cond;
  int client_fd;
  t_pcb* pcb;
} t_dispatch_fixture;

static void dispatch_fixture_start(t_dispatch_fixture* fx)
{
  fx->logger = ks_quiet_logger();
  fx->queues = ks_stub_queues_full(fx->logger);
  fx->mutex_list = init_list_mutex();
  fx->io = create_io_structures();
  fx->list_sockets_cpu = list_create();
  pthread_mutex_init(&fx->mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&fx->cpu_done_cond, NULL);
  fx->pcb = seed_ready_pcb(fx->queues, 5);

  int server_fd;
  fx->client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "cpu1", fx->client_fd));

  cr_assert(handle_new_cpu(server_fd, fx->list_sockets_cpu,
                           &fx->mutex_list_sockets_cpu, &fx->cpu_done_cond,
                           fx->logger, fx->mutex_list, fx->queues, fx->io,
                           fx->queues->km_socket, -1));
  cr_assert_eq(receive_handshake(fx->client_fd), MID_KERNEL_SCHEDULER);
  cr_assert_eq(drain_resume_pid(fx->client_fd), fx->pcb->pid);
}

/* For a syscall that does NOT preempt (PR_NO_PREEMPTION): the dispatch loop
 * just goes back to reading another op code, so shutting our end down first
 * is what drives it to close_cpu()'s default/invalid-syscall exit. */
static void dispatch_fixture_end_no_preemption(t_dispatch_fixture* fx)
{
  shutdown(fx->client_fd, SHUT_RDWR);
  close_cpu(fx->list_sockets_cpu, &fx->mutex_list_sockets_cpu,
            &fx->cpu_done_cond, fx->queues);
  close(fx->client_fd);
  reap_exec_leftover(fx->queues, fx->pcb);
  destroy_list_mutex(fx->mutex_list);
  close_io(fx->io);
  ks_wait_thread_counter_zero(fx->queues);
  ks_destroy_stub_queues_full(fx->queues);
  log_destroy(fx->logger);
}

/* For a syscall that DID preempt (the pcb left EXEC on its own): the ready
 * queue's terminate wakes the loop's next fetch attempt directly, no need to
 * shut our end down first, and there's nothing left in EXEC to reap. */
static void dispatch_fixture_end_after_preemption(t_dispatch_fixture* fx)
{
  close_cpu(fx->list_sockets_cpu, &fx->mutex_list_sockets_cpu,
            &fx->cpu_done_cond, fx->queues);
  close(fx->client_fd);
  destroy_list_mutex(fx->mutex_list);
  close_io(fx->io);
  ks_wait_thread_counter_zero(fx->queues);
  ks_destroy_stub_queues_full(fx->queues);
  log_destroy(fx->logger);
}

Test(ks_cpu_connection, mutex_create_registers_a_new_mutex)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);

  cr_assert(send_string(OP_SYSCALL_MUTEX_CREATE, "m", fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  cr_assert_eq(create_and_add_mutex(fx.mutex_list, "m", true, fx.queues),
               RM_MUTEX_NAME_ALREADY_EXISTS);

  dispatch_fixture_end_no_preemption(&fx);
}

Test(ks_cpu_connection, mutex_lock_locks_an_uncontended_mutex)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);
  cr_assert_eq(create_and_add_mutex(fx.mutex_list, "m", true, fx.queues),
               RM_MUTEX_CREATED);

  cr_assert(send_string(OP_SYSCALL_MUTEX_LOCK, "m", fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  t_pcb* other = create_pcb(EST_EXEC, 5);
  cr_assert_eq(list_mutex_lock(fx.mutex_list, "m", other), RM_WAITING_MUTEX);
  destroy_pcb(other);

  dispatch_fixture_end_no_preemption(&fx);
}

Test(ks_cpu_connection, mutex_unlock_hands_off_the_mutex)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);
  cr_assert_eq(create_and_add_mutex(fx.mutex_list, "m", true, fx.queues),
               RM_MUTEX_CREATED);
  cr_assert_eq(list_mutex_lock(fx.mutex_list, "m", fx.pcb), RM_MUTEX_LOCKED);

  cr_assert(send_string(OP_SYSCALL_MUTEX_UNLOCK, "m", fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  t_pcb* other = create_pcb(EST_EXEC, 5);
  cr_assert_eq(list_mutex_lock(fx.mutex_list, "m", other), RM_MUTEX_LOCKED);
  destroy_pcb(other);

  dispatch_fixture_end_no_preemption(&fx);
}

Test(ks_cpu_connection, memory_allocation_succeeds_when_kernel_memory_allocates)
{
  t_dispatch_fixture fx;
  fx.logger = ks_quiet_logger();
  fx.queues = ks_stub_queues_full(fx.logger);
  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);
  fx.queues->km_socket->km_socket = km_client_fd;
  int space = 1000;
  cr_assert(send_buffer(OP_FREE_MEMORY, &space, sizeof(space), km_server_fd));
  cr_assert(send_string(OP_MEMORY_ALLOCATED, "allocated", km_server_fd));

  fx.mutex_list = init_list_mutex();
  fx.io = create_io_structures();
  fx.list_sockets_cpu = list_create();
  pthread_mutex_init(&fx.mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&fx.cpu_done_cond, NULL);
  fx.pcb = seed_ready_pcb(fx.queues, 5);

  int server_fd;
  fx.client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "cpu1", fx.client_fd));
  cr_assert(handle_new_cpu(server_fd, fx.list_sockets_cpu,
                           &fx.mutex_list_sockets_cpu, &fx.cpu_done_cond,
                           fx.logger, fx.mutex_list, fx.queues, fx.io,
                           fx.queues->km_socket, -1));
  cr_assert_eq(receive_handshake(fx.client_fd), MID_KERNEL_SCHEDULER);
  cr_assert_eq(drain_resume_pid(fx.client_fd), fx.pcb->pid);

  t_syscall_memory request = {.pid = fx.pcb->pid, .segment_id = 0, .size = 100};
  cr_assert(send_buffer(OP_SYSCALL_MEM_ALLOC, &request, sizeof(request),
                        fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  dispatch_fixture_end_no_preemption(&fx);
  close(km_server_fd);
}

Test(ks_cpu_connection, memory_free_succeeds_when_kernel_memory_frees)
{
  t_dispatch_fixture fx;
  fx.logger = ks_quiet_logger();
  fx.queues = ks_stub_queues_full(fx.logger);
  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);
  fx.queues->km_socket->km_socket = km_client_fd;
  fx.queues->terminate_routines = true; /* isolates the resumption kick */
  cr_assert(send_string(OP_MEMORY_FREED, "freed", km_server_fd));

  fx.mutex_list = init_list_mutex();
  fx.io = create_io_structures();
  fx.list_sockets_cpu = list_create();
  pthread_mutex_init(&fx.mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&fx.cpu_done_cond, NULL);
  fx.pcb = seed_ready_pcb(fx.queues, 5);

  int server_fd;
  fx.client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "cpu1", fx.client_fd));
  cr_assert(handle_new_cpu(server_fd, fx.list_sockets_cpu,
                           &fx.mutex_list_sockets_cpu, &fx.cpu_done_cond,
                           fx.logger, fx.mutex_list, fx.queues, fx.io,
                           fx.queues->km_socket, -1));
  cr_assert_eq(receive_handshake(fx.client_fd), MID_KERNEL_SCHEDULER);
  cr_assert_eq(drain_resume_pid(fx.client_fd), fx.pcb->pid);

  t_syscall_memory request = {.pid = fx.pcb->pid, .segment_id = 0, .size = 0};
  cr_assert(send_buffer(OP_SYSCALL_MEM_FREE, &request, sizeof(request),
                        fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  dispatch_fixture_end_no_preemption(&fx);
  close(km_server_fd);
}

Test(ks_cpu_connection, start_process_creates_a_new_ready_pcb)
{
  t_dispatch_fixture fx;
  fx.logger = ks_quiet_logger();
  fx.queues = ks_stub_queues_full(fx.logger);
  int km_server_fd;
  int km_client_fd = ks_connected_pair(&km_server_fd);
  fx.queues->km_socket->km_socket = km_client_fd;
  cr_assert(send_string(OP_PROCESS_STARTED, "started", km_server_fd));

  fx.mutex_list = init_list_mutex();
  fx.io = create_io_structures();
  fx.list_sockets_cpu = list_create();
  pthread_mutex_init(&fx.mutex_list_sockets_cpu, NULL);
  pthread_cond_init(&fx.cpu_done_cond, NULL);
  fx.pcb = seed_ready_pcb(fx.queues, 5);

  int server_fd;
  fx.client_fd = ks_connected_pair(&server_fd);
  cr_assert(send_string(OP_ID_CPU, "cpu1", fx.client_fd));
  cr_assert(handle_new_cpu(server_fd, fx.list_sockets_cpu,
                           &fx.mutex_list_sockets_cpu, &fx.cpu_done_cond,
                           fx.logger, fx.mutex_list, fx.queues, fx.io,
                           fx.queues->km_socket, -1));
  cr_assert_eq(receive_handshake(fx.client_fd), MID_KERNEL_SCHEDULER);
  cr_assert_eq(drain_resume_pid(fx.client_fd), fx.pcb->pid);

  t_packet* packet = create_packet(OP_SYSCALL_INIT_PROC);
  packet_append_string(packet, "a.txt");
  int priority = 3;
  packet_append(packet, &priority, sizeof(priority));
  cr_assert(send_packet(packet, fx.client_fd));
  destroy_packet(packet);
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  t_pcb* spawned = transition_take_ready_next(&(fx.queues->ready));
  cr_assert_not_null(spawned);
  cr_assert_eq(spawned->priority, 3);
  destroy_pcb(spawned);

  dispatch_fixture_end_no_preemption(&fx);
  close(km_server_fd);
}

Test(ks_cpu_connection, exit_finishes_the_process)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);
  int km_peer_fd = wire_km_for_exit(fx.queues);

  cr_assert(send_string(OP_SYSCALL_EXIT, "PROCESS FINISHED", fx.client_fd));
  expect_preemption(fx.client_fd, OP_INTERRUPT, "process termination");

  /* transition_to_exit() already destroyed the pcb -- nothing left to reap
   * from exec. */
  dispatch_fixture_end_after_preemption(&fx);
  close(km_peer_fd);
}

Test(ks_cpu_connection, segmentation_fault_finishes_the_process)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);
  int km_peer_fd = wire_km_for_exit(fx.queues);

  cr_assert(send_string(OP_SEG_FAULT, "SEGMENTATION FAULT", fx.client_fd));
  expect_preemption(fx.client_fd, OP_INTERRUPT, "segmentation fault");

  dispatch_fixture_end_after_preemption(&fx);
  close(km_peer_fd);
}

Test(ks_cpu_connection, cycle_cpu_ok_reports_no_preemption)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);

  cr_assert(send_string(OP_CPU_CYCLE_OK, "OK", fx.client_fd));
  expect_preemption(fx.client_fd, OP_NO_INTERRUPT, "no preemption occurred");

  dispatch_fixture_end_no_preemption(&fx);
}

Test(ks_cpu_connection, invalid_syscall_ends_the_thread)
{
  t_dispatch_fixture fx;
  dispatch_fixture_start(&fx);

  /* Anything outside [OP_CPU_CYCLE_OK, OP_SYSCALL_EXIT] is treated as
   * invalid, ending the loop without a preemption message. */
  cr_assert(send_string(OP_HANDSHAKE, "?", fx.client_fd));

  close_cpu(fx.list_sockets_cpu, &fx.mutex_list_sockets_cpu, &fx.cpu_done_cond,
            fx.queues);
  close(fx.client_fd);
  reap_exec_leftover(fx.queues, fx.pcb);
  destroy_list_mutex(fx.mutex_list);
  close_io(fx.io);
  ks_wait_thread_counter_zero(fx.queues);
  ks_destroy_stub_queues_full(fx.queues);
  log_destroy(fx.logger);
}

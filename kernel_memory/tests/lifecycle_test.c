#include <criterion/criterion.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "kernel_memory/cleanup.h"
#include "kernel_memory/initializer.h"
#include "kernel_memory/structs.h"
#include "support.h"
#include "utils/collections/list.h"

/* ── init_main_memory ──────────────────────────────────────────────────── */

Test(km_init, init_main_memory_starts_empty)
{
  t_main_memory* memory = init_main_memory(2048, WORST, 30);
  cr_assert_eq(memory->total_size, 0);
  cr_assert_eq(memory->max_segment_size, 2048);
  cr_assert_eq(memory->compaction_delay, 30);
  cr_assert_eq(memory->allocation_strategy, WORST);
  cr_assert_eq(list_size(memory->segments), 0);
  cr_assert_eq(list_size(memory->holes), 0);
  free_main_memory(memory);
}

/* ── init_cpu_data ─────────────────────────────────────────────────────── */

Test(km_init, init_cpu_data_keeps_every_field)
{
  t_list processes;
  pthread_mutex_t pm;
  t_main_memory memory;
  pthread_mutex_t am;
  pthread_cond_t ac;
  int active = 0;

  t_cpu_data* data = init_cpu_data(11, &processes, &pm, 25, &memory, NULL,
                                   &active, &am, &ac, 12);
  cr_assert_eq(data->socket_cpu, 11);
  cr_assert_eq(data->processes, &processes);
  cr_assert_eq(data->instruction_delay, 25);
  cr_assert_eq(data->main_memory, &memory);
  cr_assert_eq(data->socket_scheduler, 12);
  cr_assert_eq(data->id, -1);
  free(data);
}

/* ── init_stick_data ──────────────────────────────────────────────────── */

Test(km_init, init_stick_data_defaults_size_and_port_to_minus_one)
{
  t_stick_data* data = init_stick_data(7, NULL, 8);
  cr_assert_eq(data->socket_stick, 7);
  cr_assert_eq(data->socket_scheduler, 8);
  cr_assert_eq(data->stick_size, -1);
  cr_assert_eq(data->stick_port, -1);
  free(data);
}

/* ── init_process ─────────────────────────────────────────────────────── */

Test(km_init, init_process_reads_every_instruction_from_the_script)
{
  char dir[] = "/tmp/km_test_scripts_XXXXXX";
  cr_assert_not_null(mkdtemp(dir));

  char script[256];
  snprintf(script, sizeof(script), "%s/proc.prc", dir);
  FILE* file = fopen(script, "w");
  fputs("NOOP\nSET AX 5\nEXIT\n", file);
  fclose(file);

  t_log* logger = km_quiet_logger();
  t_process* process = init_process(3, "proc.prc", dir, logger);
  cr_assert_not_null(process);
  cr_assert_eq(process->pid, 3);
  cr_assert_eq(process->instruction_count, 3);
  cr_assert_str_eq(process->instructions[0], "NOOP");
  cr_assert_str_eq(process->instructions[1], "SET AX 5");
  cr_assert_str_eq(process->instructions[2], "EXIT");

  free_process(process);
  log_destroy(logger);
  unlink(script);
  rmdir(dir);
}

Test(km_init, init_process_returns_null_when_the_script_is_missing)
{
  t_log* logger = km_quiet_logger();
  cr_assert_null(init_process(4, "missing.prc", "/tmp", logger));
  log_destroy(logger);
}

/* ── cleanup ──────────────────────────────────────────────────────────── */

Test(km_cleanup, free_swap_data_tolerates_a_null_argument)
{
  free_swap_data(NULL); /* must not crash */
}

Test(km_cleanup, free_main_memory_releases_segments_and_holes)
{
  t_main_memory* memory = init_main_memory(1024, BEST, 0);
  list_add(memory->segments, km_make_segment(0, 1, 0, 64));
  list_add(memory->holes, km_make_hole(64, 960));
  free_main_memory(memory); /* leak-checked under valgrind */
}

Test(km_cleanup, free_cpu_data_closes_the_socket_and_frees_the_struct)
{
  int fds[2];
  cr_assert_eq(pipe(fds), 0);

  t_cpu_data* cpu = calloc(1, sizeof(t_cpu_data));
  cpu->socket_cpu = fds[0];

  free_cpu_data(cpu); /* leak-checked under valgrind */

  cr_assert_eq(fcntl(fds[0], F_GETFD), -1);
  cr_assert_eq(errno, EBADF);
  close(fds[1]);
}

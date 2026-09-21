#include "kernel_memory/registry.h"

#include <criterion/criterion.h>

#include "kernel_memory/structs.h"
#include "utils/collections/list.h"
#include "utils/mutex.h"

static mtx_t mutex;

static void init_mutex(void)
{
  mtx_init(&mutex);
}

TestSuite(km_registry, .init = init_mutex);

Test(km_registry, list_add_mtx_appends_under_the_lock)
{
  t_list* list = list_create();
  int a = 1;
  int b = 2;
  list_add_mtx(list, &mutex, &a);
  list_add_mtx(list, &mutex, &b);
  cr_assert_eq(list_size(list), 2);
  cr_assert_eq(list_get(list, 1), &b);
  list_destroy(list);
}

Test(km_registry, find_process_matches_by_pid)
{
  t_list* processes = list_create();
  t_process p1 = {.pid = 1};
  t_process p2 = {.pid = 2};
  list_add(processes, &p1);
  list_add(processes, &p2);

  cr_assert_eq(find_process(processes, &mutex, 2), &p2);
  cr_assert_null(find_process(processes, &mutex, 9));

  list_destroy(processes);
}

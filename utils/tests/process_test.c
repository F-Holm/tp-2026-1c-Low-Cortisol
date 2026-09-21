#include "utils/process.h"

#include <criterion/criterion.h>

#include "utils/threads.h"

TestSuite(process, .timeout = 5.0);

typedef struct
{
  int process_id;
  long thread_id;
} t_ids;

static void* read_ids(void* raw_ids)
{
  t_ids* ids = raw_ids;
  ids->process_id = process_get_id();
  ids->thread_id = process_get_thread_id();
  return NULL;
}

Test(process, ids_are_positive_and_stable)
{
  cr_assert_gt(process_get_id(), 0);
  cr_assert_gt(process_get_thread_id(), 0);
  cr_assert_eq(process_get_id(), process_get_id());
  cr_assert_eq(process_get_thread_id(), process_get_thread_id());
}

Test(process, another_thread_shares_the_process_id_but_not_the_thread_id)
{
  t_ids other = {0};
  thrd_t thread;
  thrd_create(&thread, read_ids, &other);
  thrd_join(thread, NULL);

  cr_assert_eq(other.process_id, process_get_id());
  cr_assert_neq(other.thread_id, process_get_thread_id());
}

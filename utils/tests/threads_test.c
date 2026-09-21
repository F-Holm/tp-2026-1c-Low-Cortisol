#include "utils/threads.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdlib.h>

#include "utils/mutex.h"

TestSuite(threads, .timeout = 5.0);

static void* return_sentinel(void* arg)
{
  (void)arg;
  return (void*)0x2A;
}

Test(threads, thrd_join_receives_the_thread_result)
{
  thrd_t thread;
  thrd_create(&thread, return_sentinel, NULL);

  void* result;
  thrd_join(thread, &result);

  cr_assert_eq(result, (void*)0x2A);
}

Test(threads, thrd_join_with_a_null_out_param_discards_the_result)
{
  thrd_t thread;
  thrd_create(&thread, return_sentinel, NULL);

  cr_assert_eq(thrd_join(thread, NULL), 0);
}

typedef struct
{
  mtx_t* mutex;
  cnd_t* cond;
  bool* done;
} t_detach_args;

static void* signal_done(void* raw_args)
{
  t_detach_args* args = raw_args;
  mtx_lock(args->mutex);
  *args->done = true;
  cnd_signal(args->cond);
  mtx_unlock(args->mutex);
  return NULL;
}

Test(threads, thrd_detach_releases_its_own_resources)
{
  mtx_t mutex;
  cnd_t cond;
  mtx_init(&mutex);
  cnd_init(&cond);
  bool done = false;

  t_detach_args args = {&mutex, &cond, &done};
  thrd_t thread;
  thrd_create(&thread, signal_done, &args);
  cr_assert_eq(thrd_detach(thread), 0);

  mtx_lock(&mutex);
  while (!done)
    cnd_wait(&cond, &mutex);
  mtx_unlock(&mutex);

  cr_assert(done);

  cnd_destroy(&cond);
  mtx_destroy(&mutex);
}

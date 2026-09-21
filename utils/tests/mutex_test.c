#include "utils/mutex.h"

#include <criterion/criterion.h>
#include <stdbool.h>

#include "utils/threads.h"

TestSuite(mutex, .timeout = 5.0);

typedef struct
{
  mtx_t* mutex;
  int* counter;
  int increments;
} t_counter_args;

static void* increment_under_lock(void* raw_args)
{
  t_counter_args* args = raw_args;
  for (int i = 0; i < args->increments; i++)
  {
    mtx_lock(args->mutex);
    (*args->counter)++;
    mtx_unlock(args->mutex);
  }
  return NULL;
}

Test(mutex, mtx_lock_serializes_concurrent_increments)
{
  mtx_t mutex;
  mtx_init(&mutex);

  int counter = 0;
  int increments = 1000;
  t_counter_args args1 = {&mutex, &counter, increments};
  t_counter_args args2 = {&mutex, &counter, increments};

  thrd_t thread1, thread2;
  thrd_create(&thread1, increment_under_lock, &args1);
  thrd_create(&thread2, increment_under_lock, &args2);
  thrd_join(thread1, NULL);
  thrd_join(thread2, NULL);

  cr_assert_eq(counter, 2 * increments);

  mtx_destroy(&mutex);
}

typedef struct
{
  mtx_t* mutex;
  cnd_t* cond;
  bool* ready;
} t_signal_args;

static void* signal_ready(void* raw_args)
{
  t_signal_args* args = raw_args;
  mtx_lock(args->mutex);
  *args->ready = true;
  cnd_signal(args->cond);
  mtx_unlock(args->mutex);
  return NULL;
}

Test(mutex, cnd_wait_blocks_until_cnd_signal)
{
  mtx_t mutex;
  cnd_t cond;
  mtx_init(&mutex);
  cnd_init(&cond);
  bool ready = false;

  t_signal_args args = {&mutex, &cond, &ready};
  thrd_t producer;
  thrd_create(&producer, signal_ready, &args);

  mtx_lock(&mutex);
  while (!ready)
    cnd_wait(&cond, &mutex);
  mtx_unlock(&mutex);

  cr_assert(ready);

  thrd_join(producer, NULL);
  cnd_destroy(&cond);
  mtx_destroy(&mutex);
}

typedef struct
{
  mtx_t* mutex;
  cnd_t* cond;
  int* waiting_count;
  bool* go;
  int* woken_count;
} t_waiter_args;

// Shares a single mutex/cond pair for two predicates (a standard monitor
// pattern): waiters announce themselves via waiting_count before blocking on
// *go, so the main thread can deterministically wait until both are parked
// on cnd_wait before broadcasting -- no sleeps, no lost-wakeup race.
// Announcing via cnd_broadcast (not cnd_signal) matters here: both waiters
// and the main thread block on the very same cond variable for different
// predicates, so a plain signal could wake a sibling waiter instead of the
// main thread and strand it forever.
static void* wait_and_count(void* raw_args)
{
  t_waiter_args* args = raw_args;
  mtx_lock(args->mutex);
  (*args->waiting_count)++;
  cnd_broadcast(args->cond);
  while (!*args->go)
    cnd_wait(args->cond, args->mutex);
  (*args->woken_count)++;
  mtx_unlock(args->mutex);
  return NULL;
}

Test(mutex, cnd_broadcast_wakes_every_waiter)
{
  mtx_t mutex;
  cnd_t cond;
  mtx_init(&mutex);
  cnd_init(&cond);
  int waiting_count = 0;
  bool go = false;
  int woken_count = 0;

  t_waiter_args args = {&mutex, &cond, &waiting_count, &go, &woken_count};
  thrd_t waiter1, waiter2;
  thrd_create(&waiter1, wait_and_count, &args);
  thrd_create(&waiter2, wait_and_count, &args);

  mtx_lock(&mutex);
  while (waiting_count < 2)
    cnd_wait(&cond, &mutex);
  go = true;
  cnd_broadcast(&cond);
  mtx_unlock(&mutex);

  thrd_join(waiter1, NULL);
  thrd_join(waiter2, NULL);

  cr_assert_eq(woken_count, 2);

  cnd_destroy(&cond);
  mtx_destroy(&mutex);
}

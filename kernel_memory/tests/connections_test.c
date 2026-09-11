#include "kernel_memory/connections.h"

#include <criterion/criterion.h>
#include <pthread.h>
#include <stdlib.h>

#include "support.h"
#include "utils/collections/list.h"

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

Test(km_connections, compute_total_memory_sums_the_stick_sizes)
{
  t_list* sticks = list_create();
  list_add(sticks, km_make_stick(256));
  list_add(sticks, km_make_stick(512));
  cr_assert_eq(compute_total_memory(sticks, &mutex), 768);
  list_destroy_and_destroy_elements(sticks, free);
}

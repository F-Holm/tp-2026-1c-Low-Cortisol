#include "cpu/connections.h"

#include <criterion/criterion.h>
#include <stdlib.h>

#include "cpu/cpu.h"
#include "support.h"
#include "utils/collections/list.h"

Test(cpu_connections, compute_offset_sums_every_stick_size)
{
  t_list* sticks = list_create();
  cr_assert_eq(compute_offset(sticks), 0);

  list_add(sticks, cpu_make_stick(0, 100));
  list_add(sticks, cpu_make_stick(100, 250));
  list_add(sticks, cpu_make_stick(350, 40));

  cr_assert_eq(compute_offset(sticks), 390);

  list_destroy_and_destroy_elements(sticks, free);
}

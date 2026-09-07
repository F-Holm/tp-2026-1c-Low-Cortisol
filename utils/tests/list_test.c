#include "utils/collections/list.h"

#include <criterion/criterion.h>
#include <stdbool.h>
#include <stdlib.h>

static int values[] = {10, 20, 30, 40, 50};

static bool int_less_than(void* a, void* b)
{
  return *(int*)a < *(int*)b;
}

static int iterate_sum;

static void add_to_sum(void* element)
{
  iterate_sum += *(int*)element;
}

Test(list, starts_empty)
{
  t_list* list = list_create();
  cr_assert(list_is_empty(list));
  cr_assert_eq(list_size(list), 0);
  list_destroy(list);
}

Test(list, add_appends_and_returns_the_index)
{
  t_list* list = list_create();
  cr_assert_eq(list_add(list, &values[0]), 0);
  cr_assert_eq(list_add(list, &values[1]), 1);
  cr_assert_eq(list_size(list), 2);
  cr_assert_not(list_is_empty(list));
  cr_assert_eq(list_get(list, 0), &values[0]);
  cr_assert_eq(list_get(list, 1), &values[1]);
  list_destroy(list);
}

Test(list, get_out_of_range_is_null)
{
  t_list* list = list_create();
  list_add(list, &values[0]);
  cr_assert_null(list_get(list, -1));
  cr_assert_null(list_get(list, 1));
  list_destroy(list);
}

Test(list, remove_returns_the_element_and_shrinks_the_list)
{
  t_list* list = list_create();
  list_add(list, &values[0]);
  list_add(list, &values[1]);
  list_add(list, &values[2]);
  cr_assert_eq(list_remove(list, 1), &values[1]);
  cr_assert_eq(list_size(list), 2);
  cr_assert_eq(list_get(list, 1), &values[2]);
  cr_assert_null(list_remove(list, 5));
  list_destroy(list);
}

Test(list, remove_element_by_pointer)
{
  t_list* list = list_create();
  list_add(list, &values[0]);
  list_add(list, &values[1]);
  cr_assert(list_remove_element(list, &values[0]));
  cr_assert_not(list_remove_element(list, &values[0]));
  cr_assert_eq(list_size(list), 1);
  cr_assert_eq(list_get(list, 0), &values[1]);
  list_destroy(list);
}

Test(list, add_sorted_keeps_the_comparator_order)
{
  t_list* list = list_create();
  list_add_sorted(list, &values[2], int_less_than); /* 30 */
  list_add_sorted(list, &values[0], int_less_than); /* 10 */
  list_add_sorted(list, &values[4], int_less_than); /* 50 */
  list_add_sorted(list, &values[1], int_less_than); /* 20 */
  cr_assert_eq(*(int*)list_get(list, 0), 10);
  cr_assert_eq(*(int*)list_get(list, 1), 20);
  cr_assert_eq(*(int*)list_get(list, 2), 30);
  cr_assert_eq(*(int*)list_get(list, 3), 50);
  list_destroy(list);
}

Test(list, remove_and_destroy_element_frees_the_removed_element)
{
  t_list* list = list_create();
  list_add(list, strdup("keep"));
  list_add(list, strdup("drop"));
  list_remove_and_destroy_element(list, 1, free);
  cr_assert_eq(list_size(list), 1);
  cr_assert_str_eq((char*)list_get(list, 0), "keep");
  list_destroy_and_destroy_elements(list, free);
}

Test(list, remove_and_destroy_element_ignores_an_out_of_range_index)
{
  t_list* list = list_create();
  list_add(list, strdup("only"));
  list_remove_and_destroy_element(list, 9, free); /* no-op, must not crash */
  cr_assert_eq(list_size(list), 1);
  list_destroy_and_destroy_elements(list, free);
}

Test(list, iterate_visits_every_element_in_order)
{
  t_list* list = list_create();
  list_add(list, &values[0]);
  list_add(list, &values[1]);
  list_add(list, &values[2]);

  iterate_sum = 0;
  list_iterate(list, add_to_sum);
  cr_assert_eq(iterate_sum, 60);

  list_destroy(list);
}

Test(list, iterator_traverses_and_removes_while_iterating)
{
  t_list* list = list_create();
  for (int i = 0; i < 5; i++)
  {
    list_add(list, &values[i]);
  }

  t_list_iterator* iterator = list_iterator_create(list);
  int seen = 0;
  while (list_iterator_has_next(iterator))
  {
    int* value = list_iterator_next(iterator);
    seen++;
    if (*value % 20 == 0) /* drops 20 and 40 */
    {
      list_iterator_remove(iterator);
    }
  }
  list_iterator_destroy(iterator);

  cr_assert_eq(seen, 5);
  cr_assert_eq(list_size(list), 3);
  cr_assert_eq(*(int*)list_get(list, 0), 10);
  cr_assert_eq(*(int*)list_get(list, 1), 30);
  cr_assert_eq(*(int*)list_get(list, 2), 50);
  list_destroy(list);
}

Test(list, destroy_and_destroy_elements_frees_every_element)
{
  t_list* list = list_create();
  list_add(list, strdup("first"));
  list_add(list, strdup("second"));
  /* Leak-checked when the suite runs under valgrind. */
  list_destroy_and_destroy_elements(list, free);
}

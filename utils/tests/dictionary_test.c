#include "utils/collections/dictionary.h"

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Test(dictionary, put_then_get)
{
  t_dictionary* dict = dictionary_create();
  int a = 1;
  int b = 2;
  dictionary_put(dict, "alpha", &a);
  dictionary_put(dict, "beta", &b);
  cr_assert_eq(dictionary_get(dict, "alpha"), &a);
  cr_assert_eq(dictionary_get(dict, "beta"), &b);
  dictionary_destroy(dict);
}

Test(dictionary, get_missing_key_is_null)
{
  t_dictionary* dict = dictionary_create();
  cr_assert_null(dictionary_get(dict, "nope"));
  dictionary_destroy(dict);
}

Test(dictionary, has_key)
{
  t_dictionary* dict = dictionary_create();
  int value = 0;
  dictionary_put(dict, "present", &value);
  cr_assert(dictionary_has_key(dict, "present"));
  cr_assert_not(dictionary_has_key(dict, "absent"));
  dictionary_destroy(dict);
}

Test(dictionary, put_on_an_existing_key_replaces_the_value)
{
  t_dictionary* dict = dictionary_create();
  int first = 10;
  int second = 20;
  dictionary_put(dict, "key", &first);
  dictionary_put(dict, "key", &second);
  cr_assert_eq(dictionary_get(dict, "key"), &second);
  dictionary_destroy(dict);
}

Test(dictionary, holds_many_keys_that_collide_into_the_same_bucket)
{
  t_dictionary* dict = dictionary_create();
  char key[8];
  for (int i = 0; i < 100; i++)
  {
    snprintf(key, sizeof(key), "k%d", i);
    dictionary_put(dict, key, (void*)(long)i);
  }
  for (int i = 0; i < 100; i++)
  {
    snprintf(key, sizeof(key), "k%d", i);
    cr_assert_eq((long)dictionary_get(dict, key), i);
  }
  dictionary_destroy(dict);
}

Test(dictionary, destroy_and_destroy_elements_frees_every_value)
{
  t_dictionary* dict = dictionary_create();
  dictionary_put(dict, "one", strdup("value-one"));
  dictionary_put(dict, "two", strdup("value-two"));
  /* Leak-checked when the suite runs under valgrind. */
  dictionary_destroy_and_destroy_elements(dict, free);
}

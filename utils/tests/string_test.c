#include "utils/string.h"

#include <criterion/criterion.h>
#include <stdlib.h>
#include <string.h>

Test(string, duplicate_copies_content)
{
  char* copy = string_duplicate("hello");
  cr_assert_str_eq(copy, "hello");
  free(copy);
}

Test(string, duplicate_of_null_is_null)
{
  cr_assert_null(string_duplicate(NULL));
}

Test(string, is_empty_on_null_and_zero_length)
{
  cr_assert(string_is_empty(NULL));
  cr_assert(string_is_empty(""));
  cr_assert_not(string_is_empty(" "));
  cr_assert_not(string_is_empty("x"));
}

Test(string, starts_with)
{
  cr_assert(string_starts_with("kernel_scheduler", "kernel"));
  cr_assert(string_starts_with("abc", ""));
  cr_assert_not(string_starts_with("abc", "abcd"));
  cr_assert_not(string_starts_with("abc", "xyz"));
}

Test(string, equals_ignore_case)
{
  cr_assert(string_equals_ignore_case("INFO", "info"));
  cr_assert(string_equals_ignore_case("Warning", "wArNiNg"));
  cr_assert_not(string_equals_ignore_case("info", "error"));
}

Test(string, trim_removes_surrounding_whitespace)
{
  char* text = strdup("  \t spaced text  \n");
  string_trim(&text);
  cr_assert_str_eq(text, "spaced text");
  free(text);
}

Test(string, trim_of_all_whitespace_yields_empty_string)
{
  char* text = strdup("   \t\n");
  string_trim(&text);
  cr_assert_str_eq(text, "");
  free(text);
}

Test(string, array_new_is_empty_and_null_terminated)
{
  char** array = string_array_new();
  cr_assert_eq(string_array_size(array), 0);
  cr_assert_null(array[0]);
  string_array_destroy(array);
}

Test(string, split_on_every_separator)
{
  char** parts = string_split("a,b,c", ",");
  cr_assert_eq(string_array_size(parts), 3);
  cr_assert_str_eq(parts[0], "a");
  cr_assert_str_eq(parts[1], "b");
  cr_assert_str_eq(parts[2], "c");
  cr_assert_null(parts[3]);
  string_array_destroy(parts);
}

Test(string, split_without_separator_returns_the_whole_string)
{
  char** parts = string_split("abc", ",");
  cr_assert_eq(string_array_size(parts), 1);
  cr_assert_str_eq(parts[0], "abc");
  string_array_destroy(parts);
}

Test(string, split_keeps_the_empty_trailing_token)
{
  char** parts = string_split("a,", ",");
  cr_assert_eq(string_array_size(parts), 2);
  cr_assert_str_eq(parts[0], "a");
  cr_assert_str_eq(parts[1], "");
  string_array_destroy(parts);
}

Test(string, get_string_as_array_parses_a_bracketed_list)
{
  char** values = string_get_string_as_array("[ a, bb , ccc]");
  cr_assert_eq(string_array_size(values), 3);
  cr_assert_str_eq(values[0], "a");
  cr_assert_str_eq(values[1], "bb");
  cr_assert_str_eq(values[2], "ccc");
  string_array_destroy(values);
}

Test(string, get_string_as_array_handles_empty_and_null)
{
  char** empty = string_get_string_as_array("[]");
  cr_assert_eq(string_array_size(empty), 0);
  string_array_destroy(empty);

  char** from_null = string_get_string_as_array(NULL);
  cr_assert_eq(string_array_size(from_null), 0);
  string_array_destroy(from_null);
}

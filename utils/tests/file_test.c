#include "utils/file.h"

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TestSuite(file);

static long file_size(FILE* file)
{
  fseek(file, 0, SEEK_END);
  return ftell(file);
}

Test(file, file_resize_grows_the_file_with_zeros)
{
  FILE* file = tmpfile();
  cr_assert_not_null(file);

  cr_assert(file_resize(file, 4096));
  cr_assert_eq(file_size(file), 4096);

  rewind(file);
  for (int i = 0; i < 4096; i++)
    cr_assert_eq(fgetc(file), 0);

  fclose(file);
}

Test(file, file_resize_shrinks_the_file_and_flushes_pending_writes)
{
  FILE* file = tmpfile();
  cr_assert_not_null(file);

  fputs("0123456789", file);
  cr_assert(file_resize(file, 4));
  cr_assert_eq(file_size(file), 4);

  fclose(file);
}

Test(file, file_resize_to_the_same_size_keeps_it)
{
  FILE* file = tmpfile();
  cr_assert_not_null(file);

  cr_assert(file_resize(file, 128));
  cr_assert(file_resize(file, 128));
  cr_assert_eq(file_size(file), 128);

  fclose(file);
}

static FILE* file_with(const char* content)
{
  FILE* file = tmpfile();
  fputs(content, file);
  rewind(file);
  return file;
}

Test(file, file_read_line_returns_each_line_without_its_terminator)
{
  FILE* file = file_with("first\nsecond\n");

  char* first = file_read_line(file);
  char* second = file_read_line(file);
  cr_assert_str_eq(first, "first");
  cr_assert_str_eq(second, "second");
  cr_assert_null(file_read_line(file));

  free(first);
  free(second);
  fclose(file);
}

Test(file, file_read_line_returns_a_last_line_without_a_terminator)
{
  FILE* file = file_with("one\nlast");

  free(file_read_line(file));
  char* last = file_read_line(file);
  cr_assert_str_eq(last, "last");
  cr_assert_null(file_read_line(file));

  free(last);
  fclose(file);
}

Test(file, file_read_line_keeps_empty_lines)
{
  FILE* file = file_with("\n\nx\n");

  char* empty1 = file_read_line(file);
  char* empty2 = file_read_line(file);
  cr_assert_str_eq(empty1, "");
  cr_assert_str_eq(empty2, "");

  free(empty1);
  free(empty2);
  free(file_read_line(file));
  fclose(file);
}

Test(file, file_read_line_handles_lines_longer_than_its_initial_buffer)
{
  char content[1002];
  memset(content, 'a', 1000);
  content[1000] = '\n';
  content[1001] = '\0';
  FILE* file = file_with(content);

  char* line = file_read_line(file);
  cr_assert_eq(strlen(line), 1000UL);
  cr_assert_null(file_read_line(file));

  free(line);
  fclose(file);
}

Test(file, file_read_line_on_an_empty_file_returns_null)
{
  FILE* file = file_with("");
  cr_assert_null(file_read_line(file));
  fclose(file);
}

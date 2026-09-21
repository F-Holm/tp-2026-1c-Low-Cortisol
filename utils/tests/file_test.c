#include "utils/file.h"

#include <criterion/criterion.h>
#include <stdio.h>

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

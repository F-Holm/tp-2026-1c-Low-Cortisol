#include <criterion/criterion.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "support.h"
#include "swap/swap.h"

Test(swap_blocks, write_then_read_roundtrips_a_block)
{
  FILE* file = swap_sized_tmpfile(16 * 8);
  char written[16];
  memset(written, 'A', sizeof(written));

  write_block(file, 3, 16, written);

  char got[16];
  memset(got, 0, sizeof(got));
  read_block(file, 3, 16, got);

  cr_assert_eq(memcmp(got, written, 16), 0);
  fclose(file);
}

Test(swap_blocks, blocks_do_not_overlap)
{
  FILE* file = swap_sized_tmpfile(8 * 4);
  char block0[8];
  char block1[8];
  char block2[8];
  memset(block0, '0', sizeof(block0));
  memset(block1, '1', sizeof(block1));
  memset(block2, '2', sizeof(block2));

  write_block(file, 0, 8, block0);
  write_block(file, 2, 8, block2);
  write_block(file, 1, 8, block1);

  char got[8];
  read_block(file, 0, 8, got);
  cr_assert_eq(memcmp(got, block0, 8), 0);
  read_block(file, 1, 8, got);
  cr_assert_eq(memcmp(got, block1, 8), 0);
  read_block(file, 2, 8, got);
  cr_assert_eq(memcmp(got, block2, 8), 0);

  fclose(file);
}

Test(swap_blocks, writing_a_block_overwrites_its_previous_content)
{
  FILE* file = swap_sized_tmpfile(4 * 4);

  write_block(file, 1, 4, "AAAA");
  write_block(file, 1, 4, "BBBB");

  char got[4];
  read_block(file, 1, 4, got);
  cr_assert_eq(memcmp(got, "BBBB", 4), 0);

  fclose(file);
}

Test(swap_blocks, an_untouched_block_of_a_sized_file_reads_back_as_zeros)
{
  FILE* file = swap_sized_tmpfile(16 * 4);

  char got[16];
  memset(got, 0xFF, sizeof(got));
  read_block(file, 2, 16, got);

  char zeros[16] = {0};
  cr_assert_eq(memcmp(got, zeros, 16), 0);
  fclose(file);
}

Test(swap_blocks, reading_past_the_end_of_file_leaves_the_buffer_untouched)
{
  FILE* file = tmpfile();
  cr_assert_not_null(file);

  char got[8];
  memset(got, 'Z', sizeof(got));
  read_block(file, 100, 8, got); /* nothing was ever written there */

  char expected[8];
  memset(expected, 'Z', sizeof(expected));
  cr_assert_eq(memcmp(got, expected, 8), 0);
  fclose(file);
}

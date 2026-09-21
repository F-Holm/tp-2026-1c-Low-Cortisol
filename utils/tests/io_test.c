#include "utils/io.h"

#include <criterion/criterion.h>

Test(io, type_names_match_the_enum_order)
{
  cr_assert_str_eq(IO_TYPE_NAMES[IO_STDIN], "STDIN");
  cr_assert_str_eq(IO_TYPE_NAMES[IO_STDOUT], "STDOUT");
  cr_assert_str_eq(IO_TYPE_NAMES[IO_SLEEP], "SLEEP");
}

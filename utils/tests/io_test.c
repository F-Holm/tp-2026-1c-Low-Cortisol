#include <criterion/criterion.h>

#include "utils/io.h"

Test(io, type_names_match_the_enum_order)
{
  cr_assert_str_eq(IO_TYPE_NAMES[E_STDIN], "STDIN");
  cr_assert_str_eq(IO_TYPE_NAMES[E_STDOUT], "STDOUT");
  cr_assert_str_eq(IO_TYPE_NAMES[E_SLEEP], "SLEEP");
}

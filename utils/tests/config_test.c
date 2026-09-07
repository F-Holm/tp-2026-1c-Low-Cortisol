#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils/config.h"
#include "utils/string.h"

static char config_path[] = "/tmp/utils_config_test_XXXXXX";

static void write_config(void)
{
  int fd = mkstemp(config_path);
  cr_assert_neq(fd, -1, "could not create a temp config file");

  FILE* file = fdopen(fd, "w");
  fputs(
      "# a comment line\n"
      "\n"
      "  IP = 127.0.0.1  \n"
      "PORT=8080\n"
      "SCHEME = [ FIFO, RR , VRR ]\n"
      "PSEUDOCODE=/home/user/proc=1\n",
      file);
  fclose(file);
}

static void remove_config(void)
{
  unlink(config_path);
}

TestSuite(config, .init = write_config, .fini = remove_config);

Test(config, a_missing_file_yields_null)
{
  cr_assert_null(config_create("/no/such/directory/config.ini"));
}

Test(config, string_values_are_trimmed)
{
  t_config* config = config_create(config_path);
  cr_assert_not_null(config);
  cr_assert_str_eq(config_get_string_value(config, "IP"), "127.0.0.1");
  config_destroy(config);
}

Test(config, a_line_is_split_only_on_its_first_equals_sign)
{
  t_config* config = config_create(config_path);
  cr_assert_str_eq(config_get_string_value(config, "PSEUDOCODE"),
                   "/home/user/proc=1");
  config_destroy(config);
}

Test(config, int_values_are_parsed)
{
  t_config* config = config_create(config_path);
  cr_assert_eq(config_get_int_value(config, "PORT"), 8080);
  config_destroy(config);
}

Test(config, array_values_are_parsed_and_trimmed)
{
  t_config* config = config_create(config_path);
  char** schemes = config_get_array_value(config, "SCHEME");
  cr_assert_eq(string_array_size(schemes), 3);
  cr_assert_str_eq(schemes[0], "FIFO");
  cr_assert_str_eq(schemes[1], "RR");
  cr_assert_str_eq(schemes[2], "VRR");
  string_array_destroy(schemes);
  config_destroy(config);
}

Test(config, a_missing_key_yields_null)
{
  t_config* config = config_create(config_path);
  cr_assert_null(config_get_string_value(config, "ABSENT"));
  config_destroy(config);
}

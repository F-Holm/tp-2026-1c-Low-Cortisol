#include "kernel_memory/configurator.h"

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utils/config.h"

static char* write_config(void)
{
  char path[] = "/tmp/km_test_config_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1);
  FILE* file = fdopen(fd, "w");
  fputs(
      "LOG_LEVEL=INFO\n"
      "SCRIPTS_BASEPATH=/srv/scripts\n"
      "INSTRUCTION_DELAY=50\n"
      "COMPACTION_DELAY=120\n"
      "SEGMENT_MAX_SIZE=4096\n"
      "ALLOCATION_STRATEGY=WORST\n",
      file);
  fclose(file);
  return strdup(path);
}

Test(km_configurator, allocation_from_string)
{
  cr_assert_eq(allocation_from_string("BEST"), BEST);
  cr_assert_eq(allocation_from_string("WORST"), WORST);
  cr_assert_eq(allocation_from_string("FIRST"), (t_allocation_strategy)-1);
}

Test(km_configurator, the_getters_read_their_keys)
{
  char* path = write_config();
  t_config* config = init_config(path);
  cr_assert_not_null(config);

  cr_assert_str_eq(get_scripts_basepath(config), "/srv/scripts");
  cr_assert_eq(get_instruction_delay(config), 50);
  cr_assert_eq(get_compaction_delay(config), 120);
  cr_assert_eq(get_segment_max_size(config), 4096);
  cr_assert_eq(get_allocation_strategy(config), WORST);

  config_destroy(config);
  unlink(path);
  free(path);
}

Test(km_configurator, init_config_returns_null_for_a_missing_file)
{
  cr_assert_null(init_config("/no/such/kernel_memory.config"));
}

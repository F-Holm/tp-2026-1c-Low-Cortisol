#include "cpu/initializer.h"

#include <criterion/criterion.h>
#include <criterion/redirect.h>
#include <stdio.h>
#include <unistd.h>

#include "cpu/cpu.h"
#include "cpu/handlers.h"
#include "utils/collections/dictionary.h"

Test(cpu_initializer, init_module_loads_the_config_and_logger)
{
  char path[] = "/tmp/cpu_init_test_XXXXXX";
  int fd = mkstemp(path);
  cr_assert_neq(fd, -1);
  FILE* file = fdopen(fd, "w");
  fputs("LOG_LEVEL=INFO\n", file);
  fclose(file);

  t_cpu cpu = {.id = "CPU-1"};
  cr_assert(init_module(&cpu, path));
  cr_assert_not_null(cpu.config);
  cr_assert_not_null(cpu.logger);
  cr_assert_not_null(cpu.memory_sticks);

  list_destroy(cpu.memory_sticks);
  log_destroy(cpu.logger);
  config_destroy(cpu.config);
  unlink(path);
  unlink("cpu.log"); /* init_module hardcodes this filename */
}

Test(cpu_initializer, init_module_fails_on_a_missing_config)
{
  t_cpu cpu = {.id = "CPU-1"};
  cr_assert_not(init_module(&cpu, "/no/such/cpu.config"));
}

Test(cpu_initializer, check_arguments_needs_config_and_id,
     .init = cr_redirect_stdout)
{
  char* ok[] = {"cpu", "cpu.config", "CPU-1"};
  char* missing_id[] = {"cpu", "cpu.config"};
  cr_assert(check_arguments(3, ok));
  cr_assert_not(check_arguments(2, missing_id));
}

Test(cpu_initializer, register_handlers_maps_every_instruction_name)
{
  t_dictionary* handlers = dictionary_create();
  register_handlers(handlers);

  char* names[] = {"NOOP",         "SET",        "SUM",          "SUB",
                   "JNZ",          "MOV_IN",     "MOV_OUT",      "COPY_MEM",
                   "MUTEX_CREATE", "MUTEX_LOCK", "MUTEX_UNLOCK", "MEM_ALLOC",
                   "MEM_FREE",     "SLEEP",      "STDOUT",       "STDIN",
                   "INIT_PROC",    "EXIT"};

  for (int i = 0; i < 18; i++)
    cr_assert(dictionary_has_key(handlers, names[i]), "missing handler %s",
              names[i]);

  cr_assert_eq(dictionary_get(handlers, "NOOP"), handler_noop);
  cr_assert_eq(dictionary_get(handlers, "EXIT"), handler_exit);

  dictionary_destroy(handlers);
}

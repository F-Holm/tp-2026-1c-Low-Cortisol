#include "cpu/initializer.h"

#include <criterion/criterion.h>
#include <criterion/redirect.h>

#include "cpu/cpu.h"
#include "cpu/handlers.h"
#include "utils/collections/dictionary.h"

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

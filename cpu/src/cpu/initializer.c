#include "cpu/initializer.h"

#include <stdio.h>

#include "cpu/cpu.h"
#include "cpu/handlers.h"
#include "utils/log.h"

bool check_arguments(int argc, char** argv)
{
  if (argc < 3)
  {
    printf("Usage: %s [config] [id]\n", argv[0]);
    return false;
  }
  return true;
}

bool init_module(t_cpu* cpu, char* config_path)
{
  cpu->config = config_create(config_path);

  t_log_level log_level =
      log_level_from_string(config_get_string_value(cpu->config, "LOG_LEVEL"));

  cpu->logger = log_create("cpu.log", cpu->id, true, log_level, false);

  if (cpu->config == NULL)
  {
    log_error(cpu->logger, "## Could not load the config");
    return false;
  }

  if (cpu->logger == NULL)
  {
    log_error(cpu->logger, "## Could not load the logger");
    return false;
  }

  log_info(cpu->logger, "Starting CPU %s", cpu->id);
  log_info(cpu->logger, "config loaded successfully");

  cpu->memory_sticks = list_create();
  return true;
}

void register_handlers(t_dictionary* handlers)
{
  dictionary_put(handlers, "NOOP", (void*)handler_noop);
  dictionary_put(handlers, "SET", (void*)handler_set);
  dictionary_put(handlers, "SUM", (void*)handler_sum);
  dictionary_put(handlers, "SUB", (void*)handler_sub);
  dictionary_put(handlers, "JNZ", (void*)handler_jnz);
  dictionary_put(handlers, "MOV_IN", (void*)handler_mov_in);
  dictionary_put(handlers, "MOV_OUT", (void*)handler_mov_out);
  dictionary_put(handlers, "COPY_MEM", (void*)handler_copy_mem);
  dictionary_put(handlers, "MUTEX_CREATE", (void*)handler_mutex_create);
  dictionary_put(handlers, "MUTEX_LOCK", (void*)handler_mutex_lock);
  dictionary_put(handlers, "MUTEX_UNLOCK", (void*)handler_mutex_unlock);
  dictionary_put(handlers, "MEM_ALLOC", (void*)handler_mem_alloc);
  dictionary_put(handlers, "MEM_FREE", (void*)handler_mem_free);
  dictionary_put(handlers, "SLEEP", (void*)handler_sleep);
  dictionary_put(handlers, "STDOUT", (void*)handler_stdout);
  dictionary_put(handlers, "STDIN", (void*)handler_stdin);
  dictionary_put(handlers, "INIT_PROC", (void*)handler_init_proc);
  dictionary_put(handlers, "EXIT", (void*)handler_exit);
}

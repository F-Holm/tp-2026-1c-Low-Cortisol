#include "utils/log.h"

#include <criterion/criterion.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char log_dir[] = "/tmp/utils_log_test_XXXXXX";
static char log_file[64];

static void setup_log(void)
{
  cr_assert_not_null(mkdtemp(log_dir), "could not create a temp dir");
  snprintf(log_file, sizeof(log_file), "%s/module.log", log_dir);
}

static void teardown_log(void)
{
  unlink(log_file);
  rmdir(log_dir);
}

TestSuite(log, .init = setup_log, .fini = teardown_log);

static char* read_log(void)
{
  FILE* file = fopen(log_file, "r");
  cr_assert_not_null(file, "log file was not created");
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file);
  char* contents = malloc(length + 1);
  size_t read = fread(contents, 1, length, file);
  contents[read] = '\0';
  fclose(file);
  return contents;
}

Test(log, level_as_string)
{
  cr_assert_str_eq(log_level_as_string(LOG_LEVEL_TRACE), "TRACE");
  cr_assert_str_eq(log_level_as_string(LOG_LEVEL_DEBUG), "DEBUG");
  cr_assert_str_eq(log_level_as_string(LOG_LEVEL_INFO), "INFO");
  cr_assert_str_eq(log_level_as_string(LOG_LEVEL_WARNING), "WARNING");
  cr_assert_str_eq(log_level_as_string(LOG_LEVEL_ERROR), "ERROR");
}

Test(log, level_from_string_is_case_insensitive)
{
  cr_assert_eq(log_level_from_string("trace"), LOG_LEVEL_TRACE);
  cr_assert_eq(log_level_from_string("Debug"), LOG_LEVEL_DEBUG);
  cr_assert_eq(log_level_from_string("INFO"), LOG_LEVEL_INFO);
  cr_assert_eq(log_level_from_string("wArNiNg"), LOG_LEVEL_WARNING);
  cr_assert_eq(log_level_from_string("ERROR"), LOG_LEVEL_ERROR);
}

Test(log, level_from_string_rejects_an_unknown_name)
{
  cr_assert_eq(log_level_from_string("verbose"), -1);
  cr_assert_eq(log_level_from_string(""), -1);
}

Test(log, create_on_an_unreachable_path_returns_null)
{
  cr_assert_null(log_create("/no/such/directory/module.log", "test", false,
                            LOG_LEVEL_INFO, false));
}

Test(log, an_entry_carries_the_level_program_name_and_formatted_message)
{
  t_log* logger =
      log_create(log_file, "utils-test", false, LOG_LEVEL_TRACE, false);
  cr_assert_not_null(logger);
  log_info(logger, "answer is %d", 42);
  log_destroy(logger);

  char* contents = read_log();
  cr_assert_not_null(strstr(contents, "[INFO]"));
  cr_assert_not_null(strstr(contents, "utils-test"));
  cr_assert_not_null(strstr(contents, "answer is 42"));
  free(contents);
}

Test(log, every_level_helper_writes_when_the_detail_allows_it)
{
  t_log* logger =
      log_create(log_file, "utils-test", false, LOG_LEVEL_TRACE, true);
  log_trace(logger, "a-trace");
  log_debug(logger, "a-debug");
  log_info(logger, "an-info");
  log_warning(logger, "a-warning");
  log_error(logger, "an-error");
  log_destroy(logger);

  char* contents = read_log();
  cr_assert_not_null(strstr(contents, "a-trace"));
  cr_assert_not_null(strstr(contents, "a-debug"));
  cr_assert_not_null(strstr(contents, "an-info"));
  cr_assert_not_null(strstr(contents, "a-warning"));
  cr_assert_not_null(strstr(contents, "an-error"));
  free(contents);
}

Test(log, entries_below_the_detail_level_are_dropped)
{
  t_log* logger =
      log_create(log_file, "utils-test", false, LOG_LEVEL_WARNING, false);
  log_trace(logger, "hidden-trace");
  log_debug(logger, "hidden-debug");
  log_info(logger, "hidden-info");
  log_warning(logger, "shown-warning");
  log_error(logger, "shown-error");
  log_destroy(logger);

  char* contents = read_log();
  cr_assert_null(strstr(contents, "hidden-trace"));
  cr_assert_null(strstr(contents, "hidden-debug"));
  cr_assert_null(strstr(contents, "hidden-info"));
  cr_assert_not_null(strstr(contents, "shown-warning"));
  cr_assert_not_null(strstr(contents, "shown-error"));
  free(contents);
}

Test(log, a_logger_without_a_file_keeps_working)
{
  t_log* logger = log_create(NULL, "console-only", false, LOG_LEVEL_INFO, true);
  cr_assert_not_null(logger);
  cr_assert_null(logger->file);
  log_info(logger, "goes nowhere");
  log_destroy(logger);
}

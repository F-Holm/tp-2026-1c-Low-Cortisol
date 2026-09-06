#pragma once

#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>

/**
 * @file
 * @brief Minimal leveled logger, API-compatible with the subset of
 *        `commons/log.h` used by this project.
 *
 * Each entry is written as
 * `[LEVEL] HH:MM:SS:mmm PROGRAM/(pid:tid): message`.
 */

typedef enum
{
  LOG_LEVEL_TRACE,
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_ERROR
} t_log_level;

typedef struct
{
  FILE* file;
  bool is_active_console;
  t_log_level detail;
  char* program_name;
  pid_t pid;
} t_log;

/**
 * @brief Creates a logger.
 * @param file             Path of the log file (must end in `.log`), or NULL to
 *                         log only to the console.
 * @param program_name     Name shown on every entry.
 * @param is_active_console Whether entries are also printed to stdout.
 * @param detail           Lowest level that gets logged.
 * @return A new logger, or NULL on failure.
 */
t_log* log_create(char* file, char* program_name, bool is_active_console,
                  t_log_level detail);

/** @brief Closes the log file and releases the logger. */
void log_destroy(t_log* logger);

void log_trace(t_log* logger, const char* message, ...)
    __attribute__((format(printf, 2, 3)));
void log_debug(t_log* logger, const char* message, ...)
    __attribute__((format(printf, 2, 3)));
void log_info(t_log* logger, const char* message, ...)
    __attribute__((format(printf, 2, 3)));
void log_warning(t_log* logger, const char* message, ...)
    __attribute__((format(printf, 2, 3)));
void log_error(t_log* logger, const char* message, ...)
    __attribute__((format(printf, 2, 3)));

/** @brief Uppercase name of @p level (`"INFO"`, `"ERROR"`, ...). */
const char* log_level_as_string(t_log_level level);

/**
 * @brief Parses a level name (case-insensitive).
 * @return The matching level, or -1 if @p level is not recognized.
 */
t_log_level log_level_from_string(char* level);

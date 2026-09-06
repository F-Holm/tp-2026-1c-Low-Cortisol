#pragma once

#include "utils/collections/dictionary.h"

/**
 * @file
 * @brief Minimal `key=value` configuration file reader, API-compatible with the
 *        subset of `commons/config.h` used by this project.
 */

typedef struct
{
  char* path;
  t_dictionary* properties;
} t_config;

/**
 * @brief Loads the configuration file at @p path.
 * @return A new config, or NULL if the file cannot be opened.
 *
 * Blank lines and lines starting with `#` are ignored. Every other line is
 * split on its first `=` into a key and a value, both trimmed of surrounding
 * whitespace.
 */
t_config* config_create(char* path);

/** @brief Releases the config and every value it holds. */
void config_destroy(t_config* config);

/** @brief Raw string value for @p key, or NULL if the key is missing. */
char* config_get_string_value(t_config* self, char* key);

/** @brief Value for @p key parsed as an integer. */
int config_get_int_value(t_config* self, char* key);

/**
 * @brief Value for @p key parsed as a `[a, b, c]` list.
 * @return NULL-terminated array; free it with `string_array_destroy()`.
 */
char** config_get_array_value(t_config* self, char* key);

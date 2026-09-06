#ifndef UTILS_STRING_H_
#define UTILS_STRING_H_

#include <stdbool.h>

/**
 * @file
 * @brief Small string helpers, API-compatible with the subset of
 *        `commons/string.h` used by this project.
 *
 * Every `char**` array returned here is NULL-terminated and must be freed with
 * `string_array_destroy()`.
 */

/** @brief Returns a heap copy of @p original (NULL yields NULL). */
char* string_duplicate(const char* original);

/** @brief Whether @p text is NULL or has length zero. */
bool string_is_empty(const char* text);

/** @brief Whether @p text starts with @p prefix. */
bool string_starts_with(const char* text, const char* prefix);

/** @brief Case-insensitive equality. */
bool string_equals_ignore_case(const char* a, const char* b);

/** @brief Trims leading and trailing whitespace in place, reallocating. */
void string_trim(char** text);

/** @brief Creates an empty NULL-terminated string array. */
char** string_array_new(void);

/** @brief Amount of strings held by a NULL-terminated array. */
int string_array_size(char** array);

/** @brief Frees every string in the array and the array itself. */
void string_array_destroy(char** array);

/**
 * @brief Splits @p text on every occurrence of @p separator.
 * @return NULL-terminated array of the resulting tokens.
 */
char** string_split(const char* text, const char* separator);

/**
 * @brief Parses a `[a, b, c]` style value into its trimmed elements.
 * @return NULL-terminated array of the values between the brackets.
 */
char** string_get_string_as_array(const char* text);

#endif /* UTILS_STRING_H_ */

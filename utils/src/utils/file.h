#pragma once

#include <stdbool.h>
#include <stdio.h>

/**
 * @file
 * @brief Platform-agnostic file operations the C standard library lacks.
 *
 * Functions that need the operating system are backed today by
 * file_linux.c; a future Windows backend would provide them in
 * file_windows.c (guarded by #ifdef OS_WINDOWS).
 */

/**
 * @brief Sets the size of @p file to exactly @p size bytes.
 * @note Growing the file fills the new bytes with zeros; shrinking discards
 *       the rest. The file must be open for writing.
 * @return false if the file could not be resized.
 */
bool file_resize(FILE* file, long size);

/**
 * @brief Reads the next line of @p file, without its line terminator.
 * @return A newly allocated string to free with free(), or NULL at end of
 *         file (or on a read error) when nothing was read.
 */
char* file_read_line(FILE* file);

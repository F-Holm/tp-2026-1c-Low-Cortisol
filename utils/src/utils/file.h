#pragma once

#include <stdbool.h>
#include <stdio.h>

/**
 * @file
 * @brief Platform-agnostic file operations the C standard library lacks.
 *
 * Backed today by file_linux.c. A future Windows backend would provide the
 * same functions in file_windows.c (guarded by #ifdef OS_WINDOWS).
 */

/**
 * @brief Sets the size of @p file to exactly @p size bytes.
 * @note Growing the file fills the new bytes with zeros; shrinking discards
 *       the rest. The file must be open for writing.
 * @return false if the file could not be resized.
 */
bool file_resize(FILE* file, long size);

#include "utils/string.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char* string_duplicate(const char* original)
{
  if (original == NULL)
  {
    return NULL;
  }
  return strdup(original);
}

bool string_is_empty(const char* text)
{
  return text == NULL || text[0] == '\0';
}

bool string_starts_with(const char* text, const char* prefix)
{
  return strncmp(text, prefix, strlen(prefix)) == 0;
}

bool string_equals_ignore_case(const char* a, const char* b)
{
  return strcasecmp(a, b) == 0;
}

void string_trim(char** text)
{
  char* start = *text;
  while (isspace((unsigned char)*start))
  {
    start++;
  }

  size_t length = strlen(start);
  while (length > 0 && isspace((unsigned char)start[length - 1]))
  {
    length--;
  }

  char* trimmed = malloc(length + 1);
  memcpy(trimmed, start, length);
  trimmed[length] = '\0';

  free(*text);
  *text = trimmed;
}

char** string_array_new(void)
{
  char** array = malloc(sizeof(char*));
  array[0] = NULL;
  return array;
}

int string_array_size(char** array)
{
  int size = 0;
  while (array[size] != NULL)
  {
    size++;
  }
  return size;
}

void string_array_destroy(char** array)
{
  for (int i = 0; array[i] != NULL; i++)
  {
    free(array[i]);
  }
  free(array);
}

static void string_array_push(char*** array, char* value, int size)
{
  *array = realloc(*array, sizeof(char*) * (size + 2));
  (*array)[size] = value;
  (*array)[size + 1] = NULL;
}

char** string_split(const char* text, const char* separator)
{
  char** substrings = string_array_new();
  int size = 0;
  size_t separator_length = strlen(separator);

  const char* start = text;
  const char* match;
  while ((match = strstr(start, separator)) != NULL)
  {
    size_t token_length = match - start;
    char* token = malloc(token_length + 1);
    memcpy(token, start, token_length);
    token[token_length] = '\0';
    string_array_push(&substrings, token, size++);
    start = match + separator_length;
  }

  string_array_push(&substrings, strdup(start), size);
  return substrings;
}

char** string_get_string_as_array(const char* text)
{
  if (text == NULL)
  {
    return string_array_new();
  }

  size_t length = strlen(text);
  size_t inner_length = length >= 2 ? length - 2 : 0;
  if (inner_length == 0)
  {
    return string_array_new();
  }

  char* inner = malloc(inner_length + 1);
  memcpy(inner, text + 1, inner_length);
  inner[inner_length] = '\0';

  char** values = string_split(inner, ",");
  for (int i = 0; values[i] != NULL; i++)
  {
    string_trim(&values[i]);
  }

  free(inner);
  return values;
}

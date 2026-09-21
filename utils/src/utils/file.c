#include "utils/file.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const size_t INITIAL_CAPACITY = 128;

char* file_read_line(FILE* file)
{
  size_t capacity = INITIAL_CAPACITY;
  size_t length = 0;
  char* line = malloc(capacity);

  while (fgets(line + length, capacity - length, file) != NULL)
  {
    length += strlen(line + length);
    if (length > 0 && line[length - 1] == '\n')
    {
      line[length - 1] = '\0';
      return line;
    }
    if (length + 1 == capacity)
    {
      capacity *= 2;
      line = realloc(line, capacity);
    }
  }

  if (length == 0)
  {
    free(line);
    return NULL;
  }
  return line;
}

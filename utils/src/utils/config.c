#include "utils/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils/string.h"

static void config_load_line(t_config* self, char* line);

t_config* config_create(char* path)
{
  FILE* file = fopen(path, "r");
  if (file == NULL)
  {
    return NULL;
  }

  t_config* self = malloc(sizeof(t_config));
  self->path = string_duplicate(path);
  self->properties = dictionary_create();

  char* line = NULL;
  size_t capacity = 0;
  while (getline(&line, &capacity, file) != -1)
  {
    config_load_line(self, line);
  }

  free(line);
  fclose(file);
  return self;
}

void config_destroy(t_config* config)
{
  dictionary_destroy_and_destroy_elements(config->properties, free);
  free(config->path);
  free(config);
}

char* config_get_string_value(t_config* self, char* key)
{
  return dictionary_get(self->properties, key);
}

int config_get_int_value(t_config* self, char* key)
{
  return atoi(config_get_string_value(self, key));
}

char** config_get_array_value(t_config* self, char* key)
{
  return string_get_string_as_array(config_get_string_value(self, key));
}

static void config_load_line(t_config* self, char* line)
{
  char* trimmed = string_duplicate(line);
  string_trim(&trimmed);

  if (string_is_empty(trimmed) || string_starts_with(trimmed, "#"))
  {
    free(trimmed);
    return;
  }

  char* separator = strchr(trimmed, '=');
  if (separator == NULL)
  {
    free(trimmed);
    return;
  }

  *separator = '\0';
  char* key = string_duplicate(trimmed);
  char* value = string_duplicate(separator + 1);
  string_trim(&key);
  string_trim(&value);

  dictionary_put(self->properties, key, value);

  free(key);
  free(trimmed);
}

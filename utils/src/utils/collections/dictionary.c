#include "utils/collections/dictionary.h"

#include <stdlib.h>
#include <string.h>

static unsigned int dictionary_hash(const char* key);
static t_hash_element* dictionary_find(t_dictionary* self, const char* key);
static void dictionary_clean(t_dictionary* self, void (*data_destroyer)(void*));

t_dictionary* dictionary_create(void)
{
  t_dictionary* self = malloc(sizeof(t_dictionary));
  self->table_size = DICTIONARY_INITIAL_SIZE;
  self->elements = calloc(self->table_size, sizeof(t_hash_element*));
  self->elements_amount = 0;
  return self;
}

void dictionary_put(t_dictionary* self, char* key, void* data)
{
  t_hash_element* existing = dictionary_find(self, key);
  if (existing != NULL)
  {
    existing->data = data;
    return;
  }

  t_hash_element* element = malloc(sizeof(t_hash_element));
  element->key = strdup(key);
  element->hashcode = dictionary_hash(key);
  element->data = data;

  int index = element->hashcode % self->table_size;
  element->next = self->elements[index];
  self->elements[index] = element;
  self->elements_amount++;
}

void* dictionary_get(t_dictionary* self, char* key)
{
  t_hash_element* element = dictionary_find(self, key);
  return element != NULL ? element->data : NULL;
}

bool dictionary_has_key(t_dictionary* self, char* key)
{
  return dictionary_find(self, key) != NULL;
}

void dictionary_destroy(t_dictionary* self)
{
  dictionary_clean(self, NULL);
  free(self->elements);
  free(self);
}

void dictionary_destroy_and_destroy_elements(t_dictionary* self,
                                             void (*data_destroyer)(void*))
{
  dictionary_clean(self, data_destroyer);
  free(self->elements);
  free(self);
}

// Jenkins one-at-a-time hash.
static unsigned int dictionary_hash(const char* key)
{
  unsigned int hash = 0;
  for (const unsigned char* c = (const unsigned char*)key; *c != '\0'; c++)
  {
    hash += *c;
    hash += hash << 10;
    hash ^= hash >> 6;
  }
  hash += hash << 3;
  hash ^= hash >> 11;
  hash += hash << 15;
  return hash;
}

static t_hash_element* dictionary_find(t_dictionary* self, const char* key)
{
  unsigned int hashcode = dictionary_hash(key);
  int index = hashcode % self->table_size;
  for (t_hash_element* element = self->elements[index]; element != NULL;
       element = element->next)
  {
    if (element->hashcode == hashcode && strcmp(element->key, key) == 0)
    {
      return element;
    }
  }
  return NULL;
}

static void dictionary_clean(t_dictionary* self, void (*data_destroyer)(void*))
{
  for (int index = 0; index < self->table_size; index++)
  {
    t_hash_element* element = self->elements[index];
    while (element != NULL)
    {
      t_hash_element* next = element->next;
      if (data_destroyer != NULL)
      {
        data_destroyer(element->data);
      }
      free(element->key);
      free(element);
      element = next;
    }
    self->elements[index] = NULL;
  }
  self->elements_amount = 0;
}

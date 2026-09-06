#ifndef UTILS_COLLECTIONS_DICTIONARY_H_
#define UTILS_COLLECTIONS_DICTIONARY_H_

#include <stdbool.h>

/**
 * @file
 * @brief Minimal string-keyed hash map, API-compatible with the subset of
 *        `commons/collections/dictionary.h` used by this project.
 */

#define DICTIONARY_INITIAL_SIZE 20

typedef struct hash_element
{
  char* key;
  unsigned int hashcode;
  void* data;
  struct hash_element* next;
} t_hash_element;

typedef struct
{
  t_hash_element** elements;
  int table_size;
  int elements_amount;
} t_dictionary;

/** @brief Creates an empty dictionary. */
t_dictionary* dictionary_create(void);

/**
 * @brief Associates @p data with a copy of @p key. If the key was already
 *        present its value is replaced (the previous value is not freed).
 */
void dictionary_put(t_dictionary* self, char* key, void* data);

/** @brief Returns the value associated with @p key, or NULL if absent. */
void* dictionary_get(t_dictionary* self, char* key);

/** @brief Whether @p key is present. */
bool dictionary_has_key(t_dictionary* self, char* key);

/** @brief Destroys the dictionary without touching the stored values. */
void dictionary_destroy(t_dictionary* self);

/**
 * @brief Destroys the dictionary, freeing every value with @p data_destroyer.
 */
void dictionary_destroy_and_destroy_elements(t_dictionary* self,
                                             void (*data_destroyer)(void*));

#endif /* UTILS_COLLECTIONS_DICTIONARY_H_ */

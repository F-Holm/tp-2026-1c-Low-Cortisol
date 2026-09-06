#pragma once

#include <stdbool.h>

/**
 * @file
 * @brief Minimal singly linked list, API-compatible with the subset of
 *        `commons/collections/list.h` used by this project.
 */

typedef struct link_element
{
  void* data;
  struct link_element* next;
} t_link_element;

typedef struct
{
  t_link_element* head;
  int elements_count;
} t_list;

typedef struct
{
  t_list* list;
  t_link_element** current;
  t_link_element** next;
  int index;
} t_list_iterator;

/** @brief Creates an empty list. */
t_list* list_create(void);

/**
 * @brief Destroys the list without touching the elements it holds.
 */
void list_destroy(t_list* self);

/**
 * @brief Destroys the list and every element, calling @p element_destroyer
 *        on each one.
 */
void list_destroy_and_destroy_elements(t_list* self,
                                       void (*element_destroyer)(void*));

/**
 * @brief Appends @p element at the end of the list.
 * @return The index the element was inserted at.
 */
int list_add(t_list* self, void* element);

/**
 * @brief Inserts @p data keeping the order defined by @p comparator, which must
 *        return true when its first argument should appear before the second.
 * @return The index the element was inserted at.
 */
int list_add_sorted(t_list* self, void* data, bool (*comparator)(void*, void*));

/** @brief Returns the element stored at @p index, or NULL if out of range. */
void* list_get(t_list* self, int index);

/**
 * @brief Removes the element at @p index and returns it.
 */
void* list_remove(t_list* self, int index);

/**
 * @brief Removes the first node whose data pointer equals @p element.
 * @return true if an element was removed.
 */
bool list_remove_element(t_list* self, void* element);

/**
 * @brief Removes the element at @p index and frees it with @p
 * element_destroyer.
 */
void list_remove_and_destroy_element(t_list* self, int index,
                                     void (*element_destroyer)(void*));

/** @brief Calls @p closure with every element, in order. */
void list_iterate(t_list* self, void (*closure)(void*));

/** @brief Amount of elements currently stored. */
int list_size(t_list* self);

/** @brief Whether the list holds no elements. */
bool list_is_empty(t_list* self);

/**
 * @brief Creates an external iterator that allows removing elements while
 *        traversing the list. Must be freed with `list_iterator_destroy()`.
 */
t_list_iterator* list_iterator_create(t_list* list);

/** @brief Whether there are still elements left to visit. */
bool list_iterator_has_next(t_list_iterator* iterator);

/** @brief Advances to the next element and returns it. */
void* list_iterator_next(t_list_iterator* iterator);

/**
 * @brief Removes the element last returned by `list_iterator_next()` from the
 *        list. Only valid once per `list_iterator_next()` call.
 */
void list_iterator_remove(t_list_iterator* iterator);

/** @brief Releases the iterator. Does not touch the list nor its elements. */
void list_iterator_destroy(t_list_iterator* iterator);

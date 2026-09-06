#include "utils/collections/list.h"

#include <stdlib.h>

static t_link_element* list_create_element(void* data)
{
  t_link_element* element = malloc(sizeof(t_link_element));
  element->data = data;
  element->next = NULL;
  return element;
}

static t_link_element** list_get_slot(t_list* self, int index)
{
  t_link_element** slot = &self->head;
  for (int i = 0; i < index && *slot != NULL; i++)
  {
    slot = &(*slot)->next;
  }
  return slot;
}

t_list* list_create(void)
{
  t_list* self = malloc(sizeof(t_list));
  self->head = NULL;
  self->elements_count = 0;
  return self;
}

static void list_clean(t_list* self, void (*element_destroyer)(void*))
{
  t_link_element* element = self->head;
  while (element != NULL)
  {
    t_link_element* next = element->next;
    if (element_destroyer != NULL)
    {
      element_destroyer(element->data);
    }
    free(element);
    element = next;
  }
  self->head = NULL;
  self->elements_count = 0;
}

void list_destroy(t_list* self)
{
  list_clean(self, NULL);
  free(self);
}

void list_destroy_and_destroy_elements(t_list* self,
                                       void (*element_destroyer)(void*))
{
  list_clean(self, element_destroyer);
  free(self);
}

int list_add(t_list* self, void* element)
{
  t_link_element** slot = list_get_slot(self, self->elements_count);
  *slot = list_create_element(element);
  return self->elements_count++;
}

int list_add_sorted(t_list* self, void* data, bool (*comparator)(void*, void*))
{
  t_link_element** slot = &self->head;
  int index = 0;
  while (*slot != NULL && !comparator(data, (*slot)->data))
  {
    slot = &(*slot)->next;
    index++;
  }

  t_link_element* new_element = list_create_element(data);
  new_element->next = *slot;
  *slot = new_element;
  self->elements_count++;
  return index;
}

void* list_get(t_list* self, int index)
{
  if (index < 0 || index >= self->elements_count)
  {
    return NULL;
  }
  return (*list_get_slot(self, index))->data;
}

void* list_remove(t_list* self, int index)
{
  if (index < 0 || index >= self->elements_count)
  {
    return NULL;
  }

  t_link_element** slot = list_get_slot(self, index);
  t_link_element* removed = *slot;
  void* data = removed->data;
  *slot = removed->next;
  free(removed);
  self->elements_count--;
  return data;
}

bool list_remove_element(t_list* self, void* element)
{
  t_link_element** slot = &self->head;
  while (*slot != NULL)
  {
    if ((*slot)->data == element)
    {
      t_link_element* removed = *slot;
      *slot = removed->next;
      free(removed);
      self->elements_count--;
      return true;
    }
    slot = &(*slot)->next;
  }
  return false;
}

void list_remove_and_destroy_element(t_list* self, int index,
                                     void (*element_destroyer)(void*))
{
  void* data = list_remove(self, index);
  if (data != NULL)
  {
    element_destroyer(data);
  }
}

void list_iterate(t_list* self, void (*closure)(void*))
{
  for (t_link_element* element = self->head; element != NULL;
       element = element->next)
  {
    closure(element->data);
  }
}

int list_size(t_list* self)
{
  return self->elements_count;
}

bool list_is_empty(t_list* self)
{
  return self->elements_count == 0;
}

t_list_iterator* list_iterator_create(t_list* list)
{
  t_list_iterator* iterator = malloc(sizeof(t_list_iterator));
  iterator->list = list;
  iterator->current = &list->head;
  iterator->next = &list->head;
  iterator->index = -1;
  return iterator;
}

bool list_iterator_has_next(t_list_iterator* iterator)
{
  return *iterator->next != NULL;
}

void* list_iterator_next(t_list_iterator* iterator)
{
  iterator->current = iterator->next;
  iterator->index++;
  iterator->next = &(*iterator->current)->next;
  return (*iterator->current)->data;
}

void list_iterator_remove(t_list_iterator* iterator)
{
  t_link_element* removed = *iterator->current;
  *iterator->current = removed->next;
  iterator->next = iterator->current;
  iterator->index--;
  iterator->list->elements_count--;
  free(removed);
}

void list_iterator_destroy(t_list_iterator* iterator)
{
  free(iterator);
}

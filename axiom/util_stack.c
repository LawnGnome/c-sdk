#include "nr_axiom.h"
#include "util_stack.h"

bool nr_stack_init(nr_stack_t* s, size_t capacity) {
  if (NULL == s || 0 == capacity) {
    return false;
  }

  s->capacity = capacity;
  s->elements = nr_reallocarray(NULL, capacity, sizeof(void*));
  s->top = -1;
  return true;
}

void* nr_stack_get_top(nr_stack_t* s) {
  if (nrunlikely(NULL == s || nr_stack_is_empty(s))) {
    return NULL;
  }
  return s->elements[s->top];
}

void nr_stack_push(nr_stack_t* s, void* new_element) {
  if (nrunlikely(NULL == s)) {
    return;
  }

  /* The stack could be full.  If so, double it. */
  if ((size_t)((s->top) + 1) == s->capacity) {
    size_t new_capacity = s->capacity * 2;
    s->elements = nr_reallocarray(s->elements, sizeof(void*), new_capacity);
    s->capacity = new_capacity;
  }

  s->top += 1;
  s->elements[s->top] = new_element;
}

void* nr_stack_pop(nr_stack_t* s) {
  if (nrunlikely(NULL == s || nr_stack_is_empty(s))) {
    return NULL;
  }
  return s->elements[s->top--];
}

void nr_stack_destroy_fields(nr_stack_t* s) {
  if (nrunlikely(NULL == s)) {
    return;
  }

  s->capacity = 0;
  s->top = -1;
  nr_free(s->elements);
}

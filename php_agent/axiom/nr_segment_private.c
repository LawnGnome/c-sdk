#include "nr_axiom.h"

#include "nr_segment_private.h"
#include "nr_segment.h"
#include "nr_txn.h"
#include "util_memory.h"
#include "util_string_pool.h"
#include "util_time.h"

void nr_segment_children_init(nr_segment_children_t* children) {
  if (nrunlikely(NULL == children)) {
    return;
  }
  children->capacity = 8;
  children->used = 0;
  children->children
      = nr_reallocarray(NULL, children->capacity, sizeof(nr_segment_t*));
}

nr_segment_t* nr_segment_children_get_prev(nr_segment_children_t* children,
                                           nr_segment_t* child) {
  nr_segment_t* prev;
  nr_segment_t* cur;
  size_t i;

  if (nrunlikely(NULL == children || NULL == children->children
                 || NULL == child)) {
    return NULL;
  }

  /* If there are 1 or fewer children, there cannot be a child previous to the
   * one given */
  if (1 >= children->used) {
    return NULL;
  }

  for (i = 1; i < children->used; i++) {
    prev = children->children[i - 1];
    cur = children->children[i];

    if (cur == child) {
      return prev;
    }
  }

  /* The supplied child is not among the children */
  return NULL;
}

nr_segment_t* nr_segment_children_get_next(nr_segment_children_t* children,
                                           nr_segment_t* child) {
  nr_segment_t* cur;
  nr_segment_t* next;
  size_t i;

  if (nrunlikely(NULL == children || NULL == children->children
                 || NULL == child)) {
    return NULL;
  }

  /* If there are 1 or fewer children, there cannot be a child after the
   * one given */
  if (1 >= children->used) {
    return NULL;
  }

  for (i = 0; i < (children->used - 1); i++) {
    cur = children->children[i];
    next = children->children[i + 1];

    if (cur == child) {
      return next;
    }
  }

  /* The supplied child is not among the children */
  return NULL;
}

void nr_segment_children_destroy_fields(nr_segment_children_t* children) {
  if (NULL == children) {
    return;
  }
  children->capacity = 0;
  children->used = 0;
  nr_free(children->children);
}

void nr_segment_children_add(nr_segment_children_t* children,
                             nr_segment_t* child) {
  if (nrunlikely(NULL == children || NULL == children->children
                 || NULL == child)) {
    return;
  }

  while (children->used >= children->capacity) {
    size_t new_capacity = children->capacity * 2;
    children->children = nr_reallocarray(children->children, new_capacity,
                                         sizeof(nr_segment_t*));
    children->capacity = new_capacity;
  }

  children->children[children->used] = child;
  children->used += 1;
}

bool nr_segment_children_remove(nr_segment_children_t* children,
                                nr_segment_t* child) {
  size_t i;

  if (nrunlikely(NULL == children || NULL == children->children
                 || NULL == child)) {
    return false;
  }

  for (i = 0; i < children->used; i++) {
    if (children->children[i] == child) {
      if (i + 1 == children->used) {
        children->children[i] = NULL;
      } else {
        nr_memmove(&(children->children[i]), &(children->children[i + 1]),
                   sizeof(nr_segment_t*) * (children->used - i - 1));
      }
      children->used -= 1;
      return true;
    }
  }

  return false;
}

void nr_segment_datastore_destroy_fields(nr_segment_datastore_t* datastore) {
  if (nrunlikely(NULL == datastore)) {
    return;
  }

  nr_free(datastore->component);
  nr_free(datastore->sql);
  nr_free(datastore->sql_obfuscated);
  nr_free(datastore->input_query_json);
  nr_free(datastore->backtrace_json);
  nr_free(datastore->explain_plan_json);
  nr_free(datastore->instance.host);
  nr_free(datastore->instance.port_path_or_id);
  nr_free(datastore->instance.database_name);
}

void nr_segment_external_destroy_fields(nr_segment_external_t* external) {
  if (nrunlikely(NULL == external)) {
    return;
  }

  nr_free(external->transaction_guid);
  nr_free(external->uri);
  nr_free(external->library);
  nr_free(external->procedure);
}

void nr_segment_destroy_typed_attributes(
    nr_segment_type_t type,
    union _nr_segment_typed_attributes_t* attributes) {
  if (nrunlikely(NULL == attributes)) {
    return;
  }

  if (NR_SEGMENT_DATASTORE == type) {
    nr_segment_datastore_destroy_fields(&attributes->datastore);
  } else if (NR_SEGMENT_EXTERNAL == type) {
    nr_segment_external_destroy_fields(&attributes->external);
  }
}

void nr_segment_destroy_fields(nr_segment_t* segment) {
  if (nrunlikely(NULL == segment)) {
    return;
  }

  nr_free(segment->id);
  nro_delete(segment->user_attributes);
  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
}

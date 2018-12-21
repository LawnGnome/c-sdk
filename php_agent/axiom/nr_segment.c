#include "nr_axiom.h"

#include "nr_segment_private.h"
#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_txn.h"
#include "util_memory.h"
#include "util_string_pool.h"
#include "util_time.h"

nr_segment_t* nr_segment_start(nrtxn_t* txn,
                               nr_segment_t* parent,
                               const char* async_context) {
  nr_segment_t* new_segment;

  if (nrunlikely(NULL == txn)) {
    return NULL;
  }

  new_segment = nr_zalloc(sizeof(nr_segment_t));

  new_segment->color = NR_SEGMENT_WHITE;
  new_segment->type = NR_SEGMENT_CUSTOM;
  new_segment->txn = txn;
  new_segment->start_time = nr_get_time();
  new_segment->user_attributes = nro_new_hash();

  nr_segment_children_init(&new_segment->children);

  if (async_context) {
    new_segment->async_context
        = nr_string_add(txn->trace_strings, async_context);
  }

  /* If an explicit parent has been passed in, parent this newly
   * started segment with the explicit parent. Make the newly-started
   * segment a sibling of its parent's (possibly) already-existing children. */
  if (parent) {
    new_segment->parent = parent;
    nr_segment_children_add(&parent->children, new_segment);
  } /* Otherwise, the parent of this new segment is the current segment on the
       transaction */
  else {
    nr_segment_t* current_segment;
    current_segment = nr_txn_get_current_segment(txn);
    new_segment->parent = current_segment;

    if (NULL != current_segment) {
      nr_segment_children_add(&current_segment->children, new_segment);
    }
    nr_txn_set_current_segment(txn, new_segment);
  }
  return new_segment;
}

bool nr_segment_set_custom(nr_segment_t* segment) {
  if (NULL == segment) {
    return false;
  }

  if (NR_SEGMENT_CUSTOM == segment->type) {
    return true;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_CUSTOM;

  return true;
}

bool nr_segment_set_datastore(nr_segment_t* segment,
                              const nr_segment_datastore_t* datastore) {
  if (nrunlikely((NULL == segment || NULL == datastore))) {
    return false;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_DATASTORE;

  // clang-format off
  // Initialize the fields of the datastore attributes, one field per line.
  segment->typed_attributes.datastore = (nr_segment_datastore_t){
      .component = datastore->component ? nr_strdup(datastore->component) : NULL,
      .sql = datastore->sql ? nr_strdup(datastore->sql) : NULL,
      .sql_obfuscated = datastore->sql_obfuscated ? nr_strdup(datastore->sql_obfuscated) : NULL,
      .input_query_json = datastore->input_query_json ? nr_strdup(datastore->input_query_json) : NULL,
      .backtrace_json = datastore->backtrace_json ? nr_strdup(datastore->backtrace_json) : NULL,
      .explain_plan_json = datastore->explain_plan_json ? nr_strdup(datastore->explain_plan_json) : NULL,
  };

  segment->typed_attributes.datastore.instance = (nr_datastore_instance_t){
      .host = datastore->instance.host ? nr_strdup(datastore->instance.host) : NULL,
      .port_path_or_id = datastore->instance.port_path_or_id ? nr_strdup(datastore->instance.port_path_or_id) : NULL,
      .database_name = datastore->instance.database_name ? nr_strdup(datastore->instance.database_name): NULL,
  };
  // clang-format on

  return true;
}

bool nr_segment_set_external(nr_segment_t* segment,
                             const nr_segment_external_t* external) {
  if (nrunlikely((NULL == segment) || (NULL == external))) {
    return false;
  }

  nr_segment_destroy_typed_attributes(segment->type,
                                      &segment->typed_attributes);
  segment->type = NR_SEGMENT_EXTERNAL;

  // clang-format off
  // Initialize the fields of the external attributes, one field per line.
  segment->typed_attributes.external = (nr_segment_external_t){
      .transaction_guid = external->transaction_guid ? nr_strdup(external->transaction_guid) : NULL,
      .uri = external->uri ? nr_strdup(external->uri) : NULL,
      .library = external->library ? nr_strdup(external->library) : NULL,
      .procedure = external->procedure ? nr_strdup(external->procedure) : NULL,
  };
  // clang-format on

  return true;
}

bool nr_segment_add_child(nr_segment_t* parent, nr_segment_t* child) {
  if (nrunlikely((NULL == parent) || (NULL == child))) {
    return false;
  }

  nr_segment_set_parent(child, parent);

  return true;
}

bool nr_segment_set_name(nr_segment_t* segment, const char* name) {
  if ((NULL == segment) || (NULL == name)) {
    return false;
  }

  segment->name = nr_string_add(segment->txn->trace_strings, name);

  return true;
}

bool nr_segment_set_parent(nr_segment_t* segment, nr_segment_t* parent) {
  if (NULL == segment) {
    return false;
  }

  if (NULL != parent && segment->txn != parent->txn) {
    return false;
  }

  if (segment->parent == parent) {
    return true;
  }

  if (segment->parent) {
    nr_segment_children_remove(&segment->parent->children, segment);
  }

  nr_segment_children_add(&parent->children, segment);
  segment->parent = parent;

  return true;
}

bool nr_segment_set_timing(nr_segment_t* segment,
                           nrtime_t start,
                           nrtime_t duration) {
  if (NULL == segment) {
    return false;
  }

  segment->start_time = start;
  segment->stop_time = start + duration;

  return true;
}

bool nr_segment_end(nr_segment_t* segment) {
  nr_segment_t* current_segment = NULL;

  if (nrunlikely(NULL == segment) || (NULL == segment->txn)) {
    return false;
  }

  if (0 == segment->stop_time) {
    segment->stop_time = nr_get_time();
  }

  segment->txn->segment_count += 1;

  current_segment = nr_txn_get_current_segment(segment->txn);

  if (current_segment == segment) {
    nr_txn_retire_current_segment(segment->txn);
  }

  return true;
}

/*
 * Purpose : Given a segment color, return the other color.
 *
 * Params  : 1. The color to toggle.
 *
 * Returns : The toggled color.
 */
static nr_segment_color_t nr_segment_toggle_color(nr_segment_color_t color) {
  if (NR_SEGMENT_WHITE == color) {
    return NR_SEGMENT_GREY;
  } else {
    return NR_SEGMENT_WHITE;
  }
}

/*
 * Purpose : The callback necessary to iterate over a
 * tree of segments and free them and all their children.
 */
static bool nr_segment_destroy_children_callback(nr_segment_t* segment,
                                                 void* userdata NRUNUSED) {
  // If this segment has room for children, but no children,
  // then let's free the room for children.
  if (segment->children.used == 0 && segment->children.children != NULL) {
    nr_segment_children_destroy_fields(&segment->children);
  }
  return true;
}

/*
 * Purpose : Iterate over the segments in a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *           2. The color of a segment not yet traversed.
 *           3. The color of a segment already traversed.
 *           4. The iterator function to be invoked for each segment
 *           5. Optional userdata for the iterator.
 *
 * Notes   : This iterator is hardened against infinite regress. Even
 *           when there are ill-formed cycles in the tree, the
 *           iteration will terminate because it colors the segments
 *           as it traverses them.
 */
static void nr_segment_iterate_helper(nr_segment_t* root,
                                      nr_segment_color_t reset_color,
                                      nr_segment_color_t set_color,
                                      nr_segment_iter_t callback,
                                      void* userdata) {
  nrbuf_t* buf;
  size_t i;
  if (NULL == root) {
    return;
  } else {
    // Color the segments as the tree is traversed to prevent infinite regress.
    if (reset_color == root->color) {
      root->color = set_color;
      (callback)(root, userdata);

      for (i = 0; i < root->children.used; i++) {
        nr_segment_iterate_helper(root->children.children[i], reset_color,
                                  set_color, callback, userdata);
      }
      /*
       * Currently, nr_segment_iterate() and nr_segment_iterate_helper() do a
       * really good job of visiting each segment in the tree in prefix order.
       * At each segment nr_segment_iterate_helper():
       *
       * - Invokes a callback function.
       * - Recurses on the children.
       *
       * This works really well for something like "Convert this tree to a
       * min-max heap." The iterator visits each segment. At each segment, a
       * callback is invoked to copy the segment into the heap.
       *
       * This doesn't work so well if work is needed after the segment has been
       * traversed. For example, with nr_segment_traces_json_print_segments(),
       * at each segment we need it to:
       *
       * - Invoke a callback function to convert the segment to JSON.
       * - Recurse on the children, converting them to JSON.
       * - Add any closing brackets or commas after recursing on the children is
       * complete.
       *
       * Currently, nr_segment_iterate_helper() has one special case for doing
       * post-traversal work for nr_segment_traces_json_print_segments().  It
       * checks for the iterator callback used specifically by this function and
       * does the post-work.  Future versions of nr_segment_iterate_helper()
       * should allow one to register a post-traversal callback.
       */
      if (callback
          == (nr_segment_iter_t)nr_segment_traces_stot_iterator_callback) {
        buf = ((nr_segment_userdata_t*)userdata)->buf;
        nr_buffer_add(buf, "]", 1);
        nr_buffer_add(buf, "]", 1);

        /* Conditionally, we need a comma here if this node has a next sibling.
         */
        if (root->parent != NULL) {
          if (nr_segment_children_get_next(&(root->parent->children), root)) {
            nr_buffer_add(buf, ",", 1);
          }
        }
      }
      if (callback == (nr_segment_iter_t)nr_segment_destroy_children_callback) {
        /* We've reached this point because the recursion has reached the bottom
         * and now we are unrolling.  We free segments from the bottom-up.
         * If we free segments from the top down, we lose track of the child
         * pointers. */

        /* Save a pointer to the parent; it's inaccessible once the segment is freed. */
        nr_segment_t* parent = root->parent;

        /* Save a pointer to the sibling; it's inaccessible once the segment is freed.
         * Initialize the pointer to not NULL.  If nr_segment_children_get_next()
         * returns NULL, then there are no siblings */
        nr_segment_t* sibling = (nr_segment_t*)0xC0FFEE;
        if (NULL != parent) {
          sibling
              = nr_segment_children_get_next(&(root->parent->children), root);
        }

        /* Free the segment */
        nr_segment_destroy_typed_attributes(root->type,
                                            &root->typed_attributes);
        nr_free(root);

        /* If there are no more children in this family, if this is
         * the last child to be freed, then free the memory used
         * to manage the children.  Like turning a kid's bedroom
         * into a craft room when she leaves for college. */
        if (NULL == sibling) {
          nr_segment_children_destroy_fields(&parent->children);
        }
      }
      return;
    }
  }
}

void nr_segment_iterate(nr_segment_t* root,
                        nr_segment_iter_t callback,
                        void* userdata) {
  if (nrunlikely(NULL == callback)) {
    return;
  }

  if (nrunlikely(NULL == root)) {
    return;
  }
  /* What is the color of the root?  Assume the whole tree is that color.
   * The tree of segments is never partially traversed, so this assumption
   * is well-founded.
   *
   * That said, if there were a case in which the tree had been partially
   * traversed, and is traversed again, the worse case scenario would be that a
   * subset of the tree is not traversed. */
  nr_segment_iterate_helper(root, root->color,
                            nr_segment_toggle_color(root->color), callback,
                            userdata);
}

void nr_segment_destroy(nr_segment_t* root) {
  if (NULL == root) {
    return;
  }

  nr_segment_iterate(
      root, (nr_segment_iter_t)nr_segment_destroy_children_callback, NULL);
}

/*
 * This file contains data types and functions for dealing with the segments of
 * a transaction.  Historically, segments have also been called nodes, or
 * trace nodes and these three words are often used interchangeably in this
 * repository.
 *
 * This file is agent-agnostic. It defines the data types and functions used
 * to build up the multiple segments comprising a single transaction. Segments
 * may by created automatically by the agent or programmatically, by means of
 * customer API calls.
 */
#ifndef NR_SEGMENT_HDR
#define NR_SEGMENT_HDR

typedef struct _nr_segment_t nr_segment_t;
typedef struct _nrtxn_t nrtxn_t;

// TLC TODO: remove once nr_txn_save_trace_node() is removed.
typedef enum _nr_segment_type_t nr_segment_type_t;
typedef union _nr_segment_typed_attributes_t nr_segment_typed_attributes_t;

#include "nr_datastore_instance.h"
#include "nr_txn.h"
#include "util_object.h"

typedef enum _nr_segment_type_t {
  NR_SEGMENT_CUSTOM,
  NR_SEGMENT_DATASTORE,
  NR_SEGMENT_EXTERNAL
} nr_segment_type_t;

/*
 * Segment Coloring
 *
 * The C Agent API gives customers the ability to arbitrarily parent a segment
 * with any other segment. As a result, it is possible to introduce a cycle
 * into the tree. To avoid infinite regress during the recursive traversal of
 * the tree, the nodes are colored during traversal to indicate that they've
 * already been traversed.
 */
typedef enum _nr_segment_color_t {
  NR_SEGMENT_WHITE,
  NR_SEGMENT_GREY
} nr_segment_color_t;

typedef struct _nr_segment_datastore_t {
  char* component; /* The name of the database vendor or driver */
  char* sql;
  char* sql_obfuscated;
  char* input_query_json;
  char* backtrace_json;
  char* explain_plan_json;
  nr_datastore_instance_t instance;
} nr_segment_datastore_t;

typedef struct _nr_segment_external_t {
  char* transaction_guid;
  char* uri;
  char* library;
  char* procedure; /* Also known as method. */
} nr_segment_external_t;

typedef struct _nr_segment_children_t {
  size_t capacity;
  size_t used;
  nr_segment_t** children;
} nr_segment_children_t;

typedef struct _nr_segment_t {
  nr_segment_type_t type;
  nrtxn_t* txn;

  /* Tree related stuff. */
  nr_segment_t* parent;
  nr_segment_children_t children;
  nr_segment_color_t color;

  /* Generic segment fields. */
  nrtime_t start_time;      /* Start time for node */
  nrtime_t stop_time;       /* Stop time for node */
  unsigned int count;       /* N+1 rollup count */
  int name;                 /* Node name (pooled string index) */
  int async_context;        /* Execution context (pooled string index) */
  char* id;                 /* Node id.

                               If this is NULL, a new id will be created when a span
                               event is created from this trace node.

                               If this is not NULL, this id will be used for creating
                               a span event from this trace node. This id set
                               indicates that the node represents an external segment
                               and the id of the segment was use as current span id
                               in an outgoing DT payload. */
  nrobj_t* user_attributes; /* User attributes */

  /*
   * Type specific fields.
   *
   * The union type can only hold one struct at a time. This ensures that we
   * will not reserve memory for variables that are not applicable for this type
   * of node. Example: A datastore node will not need to store a method and an
   * external node will not need to store a component.
   *
   * You must check the nr_segment_type to determine which struct is being used.
   */
  union _nr_segment_typed_attributes_t {
    nr_segment_datastore_t datastore;
    nr_segment_external_t external;
  } typed_attributes;
} nr_segment_t;

/*
 * Type declaration for iterators.
 */
typedef bool (*nr_segment_iter_t)(nr_segment_t* segment, void* userdata);

/*
 * Purpose : Start a segment within a transaction's trace.
 *
 * Params  : 1. The current transaction.
 *           2. The pointer of this segment's parent, NULL if no explicit parent
 *              is known at the time of this call.  A non-NULL value is typical
 *              for API calls that support asynchronous calls.  A NULL value is
 *              typical for instrumentation of synchronous calls.
 *           3. The async_context to be applied to the segment, or NULL to
 *              indicate that the segment is not asynchronous.
 *
 * Note    : At the time of this writing, if an explicit parent is supplied
 *           then an async_context must also be supplied.  If parent is NULL
 *           and async is not NULL, or vice-versa, it can lead to undefined
 *           behavior in the agent.
 *
 * Returns : A segment.
 */
extern nr_segment_t* nr_segment_start(nrtxn_t* txn,
                                      nr_segment_t* parent,
                                      const char* async_context);

/*
 * Purpose : Destroy the fields within the given segment, without freeing the
 *           segment itself.
 *
 * Params  : 1. The segment to destroy the fields of.
 */
extern void nr_segment_destroy_fields(nr_segment_t* segment);

/*
 * Purpose : Mark the segment as being a custom segment.
 *
 * Params  : 1. The pointer to the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_custom(nr_segment_t* segment);

/*
 * Purpose : Mark the segment as being a datastore segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The datastore attributes, which will be copied into the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_datastore(nr_segment_t* segment,
                                     const nr_segment_datastore_t* datastore);

/*
 * Purpose : Mark the segment as being an external segment.
 *
 * Params  : 1. The pointer to the segment.
 *           2. The external attributes, which will be copied into the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_external(nr_segment_t* segment,
                                    const nr_segment_external_t* external);
/*
 * Purpose : Add a child to a segment.
 *
 * Params  : 1. The pointer to the parent segment.
 *           2. The pointer to the child segment.
 *
 * Notes   : If a segment, s1, is a parent of another segment, s2, that means
 *           that the instrumented code represented by s1 called into s2.
 *
 *           nr_segment_add_child() calls nr_segment_set_parent().  By itself,
 *           this function may not offer great utility, but there are times
 *           when it's easier to reason about adding a child and there are times
 *           when it's easier to reason about re-parenting a child.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_add_child(nr_segment_t* parent, nr_segment_t* child);

/*
 * Purpose : Set the name of a segment.
 *
 * Params  : 1. The pointer to the segment to be named.
 *           2. The name to be applied to the segment.
 *
 * Note    : The segment's name may be available at the start of the segment, as
 *           in cases of newrelic_start_segment(), or not until the segment's
 *           end, in cases of PHP instrumentation.  Make setting the segment
 *           name separate from starting or ending the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_name(nr_segment_t* segment, const char* name);

/*
 * Purpose : Set the parent of a segment.
 *
 * Params  : 1. The pointer to the segment to be parented.
 *           2. The pointer to the segment to become the new parent.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_parent(nr_segment_t* segment, nr_segment_t* parent);

/*
 * Purpose : Set the timing of a segment.
 *
 * Params  : 1. The pointer to the segment to be retimed.
 *           2. The new start time for the segment.
 *           3. The new duration for the segment.
 *
 * Returns : true if successful, false otherwise.
 */
extern bool nr_segment_set_timing(nr_segment_t* segment,
                                  nrtime_t start,
                                  nrtime_t duration);

/*
 * Purpose : End a segment within a transaction's trace.
 *
 * Params  : 1. The pointer to the segment to be ended.
 *
 * Returns : true if successful, false otherwise.
 *
 * Notes   : If nr_segment_set_timing() has been called, then the previously
 *           set duration will not be overriden by this function.
 *
 *           A segment can only be ended when its corresponding transaction
 *           is active.  Ending a segment after its transaction has ended
 *           will likely result in a segmentation fault.
 */
extern bool nr_segment_end(nr_segment_t* segment);

/*
 * Purpose : Destroy the fields within the given segment, without freeing the
 *           segment itself.
 *
 * Params  : 1. The segment to destroy the fields of.
 *
 */
extern void nr_segment_destroy_fields(nr_segment_t* segment);

/*
 * Purpose : Iterate over the segments in a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *           2. The iterator function to be invoked for each segment
 *           3. Optional userdata for the iterator.
 */
extern void nr_segment_iterate(nr_segment_t* root,
                               nr_segment_iter_t callback,
                               void* userdata);

/*
 * Purpose : Free a tree of segments.
 *
 * Params  : 1. A pointer to the root.
 *
 */
void nr_segment_destroy(nr_segment_t* root);

#endif /* NR_SEGMENT_HDR */

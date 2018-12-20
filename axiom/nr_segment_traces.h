#ifndef NR_SEGMENT_TRACES_HDR
#define NR_SEGMENT_TRACES_HDR

#include "nr_segment.h"
#include "nr_span_event.h"
#include "util_minmax_heap.h"
#include "util_stack.h"

/*
 * Segment Iteration Userdata
 *
 * To parametrically iterate over a tree of segments, the requisite callback
 * function takes two parameters, a pointer to a particular segment and a
 * pointer to userdata.  Traversing a segment of trees to generate a trace
 * requires that a brief list of data are available to each segment in the tree.
 * These comprise the userdata.
 */
typedef struct _nr_segment_userdata_t {
  nrbuf_t* buf;            /* The buffer to print JSON into */
  const nrtxn_t* txn;      /* The transaction, its string pool, and its pointer
                              to the root segment */
  nrpool_t* segment_names; /* The string pool for the transaction trace */
  int success; /* Was the call successful?  0 if successful, -1 if not. */
} nr_segment_userdata_t;

/*
 * Purpose : Create the internals of the transaction trace JSON expected by the
 *           collector.
 *
 * Params  : 1. The transaction.
 *           2. The duration.
 *           3. A hash representing the agent attributes.
 *           4. A hash representing the user attributes.
 *           5. A hash representing intrinsics.
 */
char* nr_segment_traces_create_data(const nrtxn_t* txn,
                                    nrtime_t duration,
                                    const nrobj_t* agent_attributes,
                                    const nrobj_t* user_attributes,
                                    const nrobj_t* intrinsics);

/*
 * Purpose : Recursively print segments to a buffer in json format.
 *
 * Params  : 1. The buffer.
 *           2. The transaction, largely for the transaction's string pool and
 *              async duration.
 *           3. The root pointer for the tree of segments.
 *           4. A string pool that the node names will be put into.  This string
 *              pool is included in the data json after the nodes:  It is used
 *              to minimize the size of the JSON.
 *
 * Returns : -1 if error, 0 otherwise.
 */
int nr_segment_traces_json_print_segments(nrbuf_t* buf,
                                          const nrtxn_t* txn,
                                          nr_segment_t* root,
                                          nrpool_t* segment_names);

/* Purpose : Create a heap of segments.
 *
 * Params  : 1. The bound for the heap, 0 if unbounded.
 *
 * Returns : A pointer to the newly-created heap.
 */
nr_minmax_heap_t* nr_segment_traces_heap_create(ssize_t bound);

/*
 * Purpose : Compare two segments.
 *
 * Params : 1. A pointer to a, the first segment for comparison.
 *          2. A pointer to b, the second segment for comparison.
 *
 * Returns : -1 if the duration of a is less than the duration of b.
 *            0 if the durations are equal.
 *            1 if the duration of a is greater than the duration of b.
 *
 * Note    : This is the comparison function required for
 *           creating a minmax heap of segments.
 */
extern int nr_segment_compare(const nr_segment_t* a, const nr_segment_t* b);

/*
 * Purpose : Place an nr_segment_t pointer into a nr_minmax_heap,
 *             or "segments to heap".
 *
 * Params  : 1. The segment pointer to place into the heap.
 *           2. A void* pointer to be recast as the pointer to the heap.
 *
 * Note    : This is the callback function supplied to nr_segment_iterate(),
 *           used for iterating over a tree of segments and placing each
 *           segment into the heap.
 */
extern void nr_segment_traces_stoh_iterator_callback(nr_segment_t* segment,
                                                     void* userdata);
/*
 * Purpose : Place an nr_segment_t pointer into a buffer.
 *             or "segments to trace".
 *
 * Params  : 1. The segment pointer to print as JSON to the buffer.
 *           2. A void* pointer to be recast as the pointer to the
 *              nr_segment_userdata_t a custom collection of data
 *              required to print one segment's worth of JSON into
 *              the buffer.
 */
extern void nr_segment_traces_stot_iterator_callback(nr_segment_t* segment,
                                                     void* userdata);
/*
 * Purpose : Given a root of a tree of segments, create a heap of
 *           segments.
 *
 * Params  : 1. A pointer to the root segment.
 *           2. A pointer to the heap to be populated.
 */
extern void nr_segment_traces_tree_to_heap(nr_segment_t* root,
                                           nr_minmax_heap_t* heap);
#endif

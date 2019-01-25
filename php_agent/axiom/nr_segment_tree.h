/*
 * This file contains functions used to access and change trees of segments.
 */

#ifndef NR_SEGMENT_TREE_H
#define NR_SEGMENT_TREE_H

#include "nr_segment.h"
#include "nr_segment_traces.h"
#include "nr_txn.h"

#include "util_set.h"

/*
 * To assemble the transaction trace, the array of span events, and the tables
 * of scoped and unscoped segment metrics, the axiom library must iterate over
 * the tree of segments, generating these as necessary.  This struct contains
 * these four products from this iteration.
 */
typedef struct {
  char* trace_json;
  nr_vector_t* span_events;
  nrmtable_t* scoped_metrics;
  nrmtable_t* unscoped_metrics;
} nr_segment_tree_result_t;

/*
 * To assemble the transaction trace and the array of span events, the axiom
 * library must iterate over the tree of segments. This struct contains the
 * input metadata and result storage for that operation. */
typedef struct {
  nr_set_t* trace_set;
  nr_set_t* span_set;
  nr_segment_tree_result_t* out;
} nr_segment_tree_sampling_metadata_t;

/*
 * Purpose : Traverse all the segments in the tree.  If a transaction trace is
 *           merited, assemble the transaction trace JSON for the highest
 *           priority segments and place it in the result struct.
 *
 * Params  : 1. A pointer to the transaction.
 *           2. A pointer to the result, possibly including both the trace JSON
 *                and the array of span events.
 *           3. The limit of segments permitted in the trace.
 *           4. The limit of span events permitted.
 *
 * Note : Future work shall yield:
 *        - The transaction's metrics.
 */
void nr_segment_tree_assemble_data(const nrtxn_t* txn,
                                   nr_segment_tree_result_t* result,
                                   const size_t trace_limit,
                                   const size_t span_limit);

/*
 * Purpose : Return a pointer to the closest sampled ancestor of the provided.
 *           segment.
 *
 * Params  : 1. The set of sampled segment.
 *           2. The starting place, the segment who's ancestors we want to learn
 *              about.
 *
 * Return  : The nearest ancestor of the passed in segment or NULL if none
 *           found.  The ancestor must be sampled, meaning that it is contained
 *           in the provided set.
 *
 */
extern nr_segment_t* nr_segment_tree_get_nearest_sampled_ancestor(
    nr_set_t* sampled_set,
    const nr_segment_t* segment);

#endif

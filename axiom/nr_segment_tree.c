#include "nr_segment_traces.h"
#include "nr_segment_tree.h"

void nr_segment_tree_assemble_data(const nrtxn_t* txn,
                                   nr_segment_tree_result_t* result,
                                   const size_t trace_limit,
                                   const size_t span_limit) {
  bool should_save_trace = false;
  bool should_sample_trace = false;
  bool should_save_spans = false;
  bool should_sample_spans = false;

  nr_segment_tree_to_heap_metadata_t first_pass_metadata = {
      .trace_heap = NULL,
      .span_heap = NULL,
      .scoped_metrics = NULL,
      .unscoped_metrics = NULL,
  };
  nr_segment_tree_sampling_metadata_t metadata
      = {.trace_set = NULL, .span_set = NULL};

  nrobj_t* agent_attributes;
  nrobj_t* user_attributes;
  nrtime_t duration;

  if (NULL == txn || NULL == result || NULL == txn->segment_root) {
    return;
  }

  metadata.out = result;

  result->scoped_metrics = first_pass_metadata.scoped_metrics
      = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);
  result->unscoped_metrics = first_pass_metadata.unscoped_metrics
      = nrm_table_create(NR_METRIC_DEFAULT_LIMIT);

  duration = nr_time_duration(txn->segment_root->start_time,
                              txn->segment_root->stop_time);

  should_save_trace
      = (trace_limit > 0) && nr_txn_should_save_trace(txn, duration);
  should_sample_trace = txn->segment_count > trace_limit;

  should_save_spans = (span_limit > 0) && nr_txn_should_create_span_events(txn);
  should_sample_spans = txn->segment_count > span_limit;

  if (should_save_spans && should_sample_spans) {
    first_pass_metadata.span_heap = nr_segment_heap_create(
        span_limit, nr_segment_wrapped_duration_comparator);
  }
  if (should_save_trace && should_sample_trace) {
    first_pass_metadata.trace_heap = nr_segment_heap_create(
        trace_limit, nr_segment_wrapped_duration_comparator);
  }

  /*
   * Generate the two heaps of traces and span events that represent the
   * highest-priority segments for this transaction.
   */
  nr_segment_tree_to_heap(txn->segment_root, &first_pass_metadata);

  /*
   * Do the first iteration of the tree.
   *
   * Future work will see metric generation in the first iteration of
   * the tree.
   */

  if (should_save_trace || should_save_spans) {
    if (should_sample_trace) {
      /* Prepare for the second pass of the tree; convert the heap to a set. */
      metadata.trace_set = nr_set_create();
      nr_segment_heap_to_set(first_pass_metadata.trace_heap,
                             metadata.trace_set);
    }

    if (should_sample_spans) {
      /* Prepare for the second pass of the tree; convert the heap to a set. */
      metadata.span_set = nr_set_create();
      nr_segment_heap_to_set(first_pass_metadata.span_heap, metadata.span_set);
    }

    agent_attributes = nr_attributes_agent_to_obj(
        txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);
    user_attributes = nr_attributes_user_to_obj(
        txn->attributes, NR_ATTRIBUTE_DESTINATION_TXN_TRACE);

    nr_segment_traces_create_data(txn, duration, &metadata, agent_attributes,
                                  user_attributes, txn->intrinsics,
                                  should_save_trace, should_save_spans);
    result->trace_json = metadata.out->trace_json;
    result->span_events = metadata.out->span_events;

    nro_delete(agent_attributes);
    nro_delete(user_attributes);

    nr_set_destroy(&metadata.trace_set);

    nr_set_destroy(&metadata.span_set);
    nr_minmax_heap_destroy(&first_pass_metadata.trace_heap);
    nr_minmax_heap_destroy(&first_pass_metadata.span_heap);
  }

  return;
}

nr_segment_t* nr_segment_tree_get_nearest_sampled_ancestor(
    nr_set_t* sampled_set,
    const nr_segment_t* segment) {
  if (NULL == segment || NULL == sampled_set || NULL == segment->txn) {
    return NULL;
  } else {
    nr_segment_t* root = segment->txn->segment_root;
    nr_segment_t* current = segment->parent;

    while (NULL != current) {
      if (nr_set_contains(sampled_set, current)) {
        return current;
      }
      // If a cycle was created in the tree it will be lost unless root is given
      // a parent. This clause will prevent infinite looping.
      if (current == root) {
        return NULL;
      }
      current = current->parent;
    }
  }

  return NULL;
}

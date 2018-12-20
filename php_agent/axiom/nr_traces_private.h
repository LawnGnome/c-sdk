/*
 * This file contains internal data structures for transaction traces.
 */
#ifndef NR_TRACES_PRIVATE_HDR
#define NR_TRACES_PRIVATE_HDR

/*
 * This header file exposes internal functions that are only made visible for
 * unit testing. Other clients are forbidden.
 */

/*
 * A utility struct used to track execution context start times on a per-node
 * basis.
 */
typedef struct _nr_harvest_trace_node_t {
  nrtime_t context_start;  /* The start time for the node's context */
  const nrtxnnode_t* node; /* The node itself */
} nr_harvest_trace_node_t;

extern int nr_traces_json_print_segments(nrbuf_t* buf,
                                         const nrtxn_t* txn,
                                         const nrtxnnode_t* node,
                                         int next,
                                         const nr_harvest_trace_node_t* nodes,
                                         nrpool_t* node_names,
                                         nr_span_event_t* span_events[],
                                         int span_events_size,
                                         const nr_span_event_t* parent_span);

/*
 * Purpose : Sorts the nodes within the transaction, taking execution contexts
 *           into account.
 *
 * Params  : 1. The transaction to sort the nodes on.
 *
 * Returns : An array of size txn->nodes_used in the correct sorted order for
 *           printing. Note that while the caller is responsible for freeing
 *           the array with nr_free, the node pointers within the array point
 *           back into the transaction, so if the transaction struct is
 *           destroyed before the array bad things will probably happen.
 */
extern nr_harvest_trace_node_t* nr_harvest_trace_sort_nodes(const nrtxn_t* txn);

#endif /* NR_TRACES_PRIVATE_HDR */

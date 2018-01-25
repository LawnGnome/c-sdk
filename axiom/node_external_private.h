#ifndef NODE_EXTERNAL_PRIVATE_HDR
#define NODE_EXTERNAL_PRIVATE_HDR

#include "nr_txn.h"

/*
 * Purpose : This function is designed to avoid repetition in the transaction
 *           trace by collapsing adjacent calls to the same function.
 *
 * Params  : 1. The current transaction.
 *           2. The start time of the just completed call.
 *           3. The stop time of the just completed call.
 *           4. The metric/node name of the just completed call.
 *
 * Returns : NR_FAILURE if the call could not be collapsed into the last
 *           added transaction trace node, or NR_SUCCESS if the collapse was
 *           successful and a new node should not be created.
 *
 * Notes   : This function has a problem:  If rollup occurs, the priority queue
 *           is not updated with the node's new duration.
 */
extern nr_status_t
nr_txn_node_rollup (nrtxn_t *txn,
                    const nrtxntime_t *start, const nrtxntime_t *stop,
                    const char *name);

#endif /* NODE_EXTERNAL_PRIVATE_HDR */

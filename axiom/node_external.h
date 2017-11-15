/*
 * This file is used to process external calls.
 */
#ifndef NODE_EXTERNAL_HDR
#define NODE_EXTERNAL_HDR

#include "nr_txn.h"
#include "util_time.h"

/*
 * Purpose : Terminate an external services call node.
 *
 * Params  : 1. The transaction pointer.
 *           2. The start time structure.
 *           3. The URL of the external service. May not be null-terminated.
 *           4. The length of the URL.
 *           5. Whether or not to roll up adjacent nodes.
 *           6. The encoded contents of the X-NewRelic-App-Data header.
 *
 * Returns : Nothing.
 *
 * Notes   : This is a node termination function for external nodes.
 *           It is called after executing a cURL/etc function in the agent
 *           and is used to determine whether or not the node is kept as part
 *           of the TT.
 */
extern void nr_txn_end_node_external (
  nrtxn_t *txn,
  const nrtxntime_t *start,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header);

/*
 * Purpose : Terminate an asynchronous external services call node.
 *
 * Params  : 1. The transaction pointer.
 *           2. The execution context.
 *           3. The start time structure.
 *           4. The duration of the call in microseconds.
 *           5. The URL of the external service. May not be null-terminated.
 *           6. The length of the URL.
 *           7. Whether or not to roll up adjacent nodes.
 *           8. The encoded contents of the X-NewRelic-App-Data header.
 *
 * Returns : Nothing.
 *
 * Notes   : This is a node termination function for external nodes.
 *           It is called after executing a cURL/etc function in the agent
 *           and is used to determine whether or not the node is kept as part
 *           of the TT.
 */
extern void
nr_txn_end_node_external_async (
  nrtxn_t *txn,
  const char *async_context,
  const nrtxntime_t *start,
  nrtime_t duration,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header);

/*
 * Purpose : Same as the two functions above, but the stop time is provided
 *           as a parameter.
 */
extern void
nr_txn_do_end_node_external (
  nrtxn_t *txn,
  const char *async_context,
  const nrtxntime_t *start,
  const nrtxntime_t *stop,
  const char *url,
  int urllen,
  int do_rollup,
  const char *encoded_response_header);

extern nr_status_t nr_txn_node_rollup (nrtxn_t *txn, const nrtxntime_t *start, const nrtxntime_t *stop, const char *name);

#endif /* NODE_EXTERNAL_HDR */

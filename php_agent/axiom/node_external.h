/*
 * This file is used to process external calls.
 */
#ifndef NODE_EXTERNAL_HDR
#define NODE_EXTERNAL_HDR

#include "nr_txn.h"
#include "util_time.h"

/*
 * Input parameters when ending an external node.
 */
typedef struct _nr_node_external_params {
  nrtxntime_t start;             /* The call start. */
  nrtxntime_t stop;              /* The call stop. */
  char* library;                 /* The null-terminated library; if unset, this
                                    is ignored. */
  char* procedure;               /* The null-terminated procedure (or method);
                                    if unset, this is ignored. */
  char* url;                     /* The URL. */
  size_t urllen;                 /* The length of the URL. */
  char* async_context;           /* The async context, if any. */
  bool do_rollup;                /* If true, adjacent nodes of the same type
                                    will be rolled up into a single node. */
  char* encoded_response_header; /* The encoded contents of the
                                    X-NewRelic-App-Data header. */
} nr_node_external_params_t;

/*
 * Purpose : Record metrics and a transaction trace node for an external call.
 *
 * Params  : 1. The current transaction.
 *           2. The parameters listed above.
 */
extern void nr_txn_end_node_external(nrtxn_t* txn,
                                     const nr_node_external_params_t* params);

#endif /* NODE_EXTERNAL_HDR */

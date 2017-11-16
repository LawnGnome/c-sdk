/*
 * This file contains functions to store and format slow transaction traces.
 */
#ifndef NR_TRACES_HDR
#define NR_TRACES_HDR

#include "nr_txn.h"
#include "util_object.h"
#include "util_time.h"

/*
 * Purpose : Create the internals of the transaction trace JSON expected by the
 *           collector.
 */
extern char *
nr_harvest_trace_create_data (
  const nrtxn_t *txn,
  nrtime_t       duration,
  const nrobj_t *agent_attributes,
  const nrobj_t *user_attributes,
  const nrobj_t *intrinsics);

#endif /* NR_TRACES_HDR */

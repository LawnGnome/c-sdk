/*
 * This file contains functions to store and format slow transaction traces.
 */
#ifndef NR_TRACES_HDR
#define NR_TRACES_HDR

#include "nr_txn.h"
#include "nr_span_event.h"
#include "util_object.h"
#include "util_time.h"

/*
 * Purpose : Create the internals of the transaction trace JSON expected by the
 *           collector.
 *
 *           As a side effect the span event tree is created based on the
 *           transaction traces. The span event buffer span_events has to be
 *           either a buffer of size (NR_TXN_MAX_EVENTS + 1) or NULL.
 */
extern char* nr_harvest_trace_create_data(const nrtxn_t* txn,
                                          nrtime_t duration,
                                          const nrobj_t* agent_attributes,
                                          const nrobj_t* user_attributes,
                                          const nrobj_t* intrinsics,
                                          nr_span_event_t* span_events[],
                                          int span_events_size);

#endif /* NR_TRACES_HDR */

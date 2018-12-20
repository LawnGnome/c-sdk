#ifndef PHP_AGENT_NR_SPAN_EVENTS_PRIVATE_H
#define PHP_AGENT_NR_SPAN_EVENTS_PRIVATE_H

#include "util_sampling.h"
#include "util_time.h"

/*
 * Span Events
 *
 * For a full agent spec, please refer to:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Span-Events.md
 *
 * These fields will not be accessed directly -- instead use the functions
 * in nr_span_events.h
 */

typedef enum {
  NR_SPAN_HTTP,
  NR_SPAN_DATASTORE,
  NR_SPAN_GENERIC
} nr_span_category_t;

struct _nr_span_events_t {
  char* trace_id;  /* Link together all spans within a distributed trace */
  char* guid;      /* The segment identifier */
  char* parent_id; /* the segment's parent's guid (may be omitted for the root
                      span) */
  char* transaction_id;            /* the transaction's guid */
  bool sampled;                    /* Whether this trip should be sampled */
  nr_sampling_priority_t priority; /* Likelihood to be saved */
  nrtime_t
      timestamp; /* Unix timestamp in milliseconds when this segment started */
  nrtime_t duration;                /* Elapsed time in seconds */
  char* name;                       /* Segment name */
  nr_span_category_t span_category; /* Span Category */
  bool is_entry_point; /* This is always true and only applied to the first
                          segment */
};

#endif /* PHP_AGENT_NR_SPAN_EVENTS_PRIVATE_H */

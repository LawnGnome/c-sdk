/*
 * This file contains functions to store and format event data for analytics
 * product.
 *
 * For a full agent spec, please refer to:
 * https://source.datanerd.us/agents/agent-specs/blob/master/Transaction-Events-PORTED.md
 */
#ifndef NR_ANALYTICS_EVENTS_HDR
#define NR_ANALYTICS_EVENTS_HDR

#include "util_object.h"
#include "util_random.h"

/*
 * This structure represents a single event.  One of these structures is
 * created per transaction.
 */
typedef struct _nr_analytics_event_t nr_analytics_event_t;

/*
 * This is a pool of events.  Each application's harvest structure holds
 * one of these.
 */
typedef struct _nr_analytics_events_t nr_analytics_events_t;

/*
 * Purpose : Create a new analytics event.
 *
 * Params  : 1. Normal fields such as 'type' and 'duration'.
 *           2. Attributes created by the user using an API call.
 *           3. Attributes created by the agent.
 *
 *           The values in these hashes should be strings, doubles
 *           and longs.  As of 12/13/13, Dirac supports two attributes types:
 *           strings and 32 bit floats.  The conversion to a 32 bit float (and
 *           possible loss of precision) is done at the collector.
 */
nr_analytics_event_t *
nr_analytics_event_create (
  const nrobj_t *builtin_fields,
  const nrobj_t *agent_attributes,
  const nrobj_t *user_attributes);

/*
 * Purpose : Destroy an analytics event, releasing all of its memory.
 *
 */
extern void nr_analytics_event_destroy (nr_analytics_event_t **event_ptr);

/*
 * Purpose : Create a data structure to hold analytics event data.
 *
 * Params  : 1. The total number of events that will be recorded.  After this
 *              maximum is reached, events will be saved/replaced randomly
 *              using a sampling algorithm.
 *
 * Returns : A newly allocated events structure, or 0 on error.
 */
extern nr_analytics_events_t *nr_analytics_events_create (int max_events);

/*
 * Purpose : Get the number of events that were attempted to be put in the
 *           structure using add_event.
 */
extern int nr_analytics_events_number_seen (const nr_analytics_events_t *events);

/*
 * Purpose : Get the number of events saved within the structure.
 */
extern int nr_analytics_events_number_saved (const nr_analytics_events_t *events);

/*
 * Purpose : Destroy an analytics event data structure, freeing all of its
 *           associated memory. 
 */
extern void nr_analytics_events_destroy (nr_analytics_events_t **events_ptr);

/*
 * Purpose : Add an event to an event pool.
 *
 * Notes   : This function is non-deterministic: Once the events data structure
 *           is full, this event may replace an existing event based upon
 *           a random number generated using the time and nrand48. 
 *
 * IMPORTANT: The number of attributes will increase the JSON size.  This
 *            should be noted when choosing max_events.
 */
extern void nr_analytics_events_add_event (
  nr_analytics_events_t      *events,
  const nr_analytics_event_t *event,
  nr_random_t *rnd);

/*
 * Purpose : Get event JSON from an event pool.
 */
extern const char *
nr_analytics_events_get_event_json (nr_analytics_events_t *events, int i);

/*
 * Purpose : Return a JSON representation of the event in the format expected
 *           by the collector.
 */
extern const char *
nr_analytics_event_json (const nr_analytics_event_t *event);

#endif /* NR_ANALYTICS_EVENTS_HDR */

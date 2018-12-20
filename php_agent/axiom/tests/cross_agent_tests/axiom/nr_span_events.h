#ifndef PHP_AGENT_NR_SPAN_EVENTS_H
#define PHP_AGENT_NR_SPAN_EVENTS_H

#include "util_sampling.h"
#include "util_time.h"

/*
 * Purpose : The public version of the distributed trace structs/types
 */
typedef struct _nr_span_events_t nr_span_events_t;

/*
 * Purpose : Creates/allocates a new span event metadata struct
 *           instance. It's the responsibitity of the caller to
 *           free/destroy the struct with the nr_span_events_destroy
 *           function.
 *
 * Params  : None.
 *
 * Returns : An allocated nr_span_events_t that the caller owns and must
 *           destroy with nr_span_events_destroy().
 */
nr_span_events_t* nr_span_events_create(void);

/*
 * Purpose : Destroys/frees structs created via nr_span_events_create.
 *
 * Params  : A pointer to the pointer that points at the allocated
 *           nr_span_events_t (created with nr_span_events_create).
 *
 * Returns : Nothing.
 */
void nr_span_events_destroy(nr_span_events_t** ptr);


#endif  /* PHP_AGENT_NR_SPAN_EVENTS_H */

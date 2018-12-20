#include "nr_axiom.h"

#include "nr_span_events.h"
#include "nr_span_events_private.h"
#include "util_memory.h"

nr_span_events_t* nr_span_events_create(void) {
    nr_span_events_t* se;
    se = (nr_span_events_t*)nr_zalloc(sizeof(nr_span_events_t));

    return se;
}

void nr_span_events_destroy(nr_span_events_t** ptr) {
    nr_span_events_t* event = NULL;

    if((NULL == ptr) || (NULL == *ptr)) {
        return;
    }

    event = *ptr;
    nr_free(event->trace_id);
    nr_free(event->guid);
    nr_free(event->parent_id);
    nr_free(event->transaction_id);
    nr_free(event->name);

    nr_realfree((void**)ptr);
}
